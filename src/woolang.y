/*
 * Woolang LALR(1) grammar, processed by byacc (public domain).
 *
 * Regenerate with:
 *     cmake -S . -B <build-dir> -DWO_REGENERATE_PARSER=ON
 *     cmake --build <build-dir> --target regenerate_parser
 * (the switch defaults to OFF; the committed parser is used otherwise), or
 * directly (from src/):
 *     byacc -l -v -H woolang.tab.h -b woolang -o woolang.tab.inc woolang.y
 *
 * The generated woolang.tab.inc is compiled through woolang.tab.cpp, which
 * includes it inside namespace wo::parser; yylex/yyerror are declared in
 * wo_lang_parser.hpp.
 *
 * Terminals are declared with explicit values `lex_type + 300`; the lexer
 * never emits l_empty any more, so optional constructs use empty rules.
 *
 * Known LALR boundary changes:
 *   - An empty `{}` in *statement* position is parsed as an empty block;
 *     write `({})` when a bare empty map expression is intended.
 *   - A tuple type's trailing comma after a variadic item, `(T, ...,)`, is no
 *     longer accepted; `(T,)` and `(T, ...)` still are.
 */
%{
#include "wo_lang_parser.hpp"

#define YYSTYPE ::wo::parser::value

/* Variadic: an A_APPEND_* macro expands to a template-id, and the comma in
 * that template argument list reaches BID as an argument separator. */
#define BID(...)        (::wo::parser::builder_id< ::wo::ast::__VA_ARGS__ >())
#define R0(B)           (::wo::parser::reduce(BID(B), *ctx, {}))
#define R1(B, A1)       (::wo::parser::reduce(BID(B), *ctx, { A1 }))
#define R2(B, A1, A2)   (::wo::parser::reduce(BID(B), *ctx, { A1, A2 }))
#define R3(B, A1, A2, A3) \
    (::wo::parser::reduce(BID(B), *ctx, { A1, A2, A3 }))
#define R4(B, A1, A2, A3, A4) \
    (::wo::parser::reduce(BID(B), *ctx, { A1, A2, A3, A4 }))
#define R5(B, A1, A2, A3, A4, A5) \
    (::wo::parser::reduce(BID(B), *ctx, { A1, A2, A3, A4, A5 }))
#define R6(B, A1, A2, A3, A4, A5, A6) \
    (::wo::parser::reduce(BID(B), *ctx, { A1, A2, A3, A4, A5, A6 }))
#define R7(B, A1, A2, A3, A4, A5, A6, A7) \
    (::wo::parser::reduce(BID(B), *ctx, { A1, A2, A3, A4, A5, A6, A7 }))
#define R8(B, A1, A2, A3, A4, A5, A6, A7, A8) \
    (::wo::parser::reduce(BID(B), *ctx, { A1, A2, A3, A4, A5, A6, A7, A8 }))
#define R9(B, A1, A2, A3, A4, A5, A6, A7, A8, A9) \
    (::wo::parser::reduce(BID(B), *ctx, { A1, A2, A3, A4, A5, A6, A7, A8, A9 }))
#define R10(B, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10) \
    (::wo::parser::reduce(BID(B), *ctx, { A1, A2, A3, A4, A5, A6, A7, A8, A9, A10 }))
#define R11(B, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11) \
    (::wo::parser::reduce(BID(B), *ctx, { A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11 }))
#define R12(B, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12) \
    (::wo::parser::reduce(BID(B), *ctx, { A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12 }))

/* pass_append_list<from, to> has a comma in its template argument list, so it
 * cannot be written directly in an R<n> call; alias it so the R<n> macros see
 * a single argument. BID re-joins the two pieces after this expansion. */
#define A_APPEND_1_0 pass_append_list<1, 0>
#define A_APPEND_2_0 pass_append_list<2, 0>
#define A_APPEND_0_1 pass_append_list<0, 1>
#define A_APPEND_1_2 pass_append_list<1, 2>
#define A_APPEND_1_3 pass_append_list<1, 3>
%}

%pure-parser
%parse-param { ::wo::parser::context *ctx }
%lex-param { ::wo::parser::context *ctx }
%start PROGRAM_AUGMENTED

/* Binding an `else` to the nearest `if` (the old grammar encoded this with
 * l_empty's lower priority; yacc expresses it with precedence). */
%nonassoc LOWER_THAN_ELSE
%nonassoc LOWER_THAN_RIGHT_CURLY
%nonassoc LOWER_THAN_LEFT_BRACKETS
%nonassoc LOWER_THAN_LESS
%nonassoc T_l_else
%nonassoc T_l_right_curly_braces
%nonassoc T_l_left_brackets
%nonassoc T_l_less

%expect 0

/* 99 terminals used by the grammar; values = lex_type + 300. */
%token T_l_add 314
%token T_l_add_assign 320
%token T_l_alias 359
%token T_l_as 360
%token T_l_assign 319
%token T_l_at 361
%token T_l_bind_monad 352
%token T_l_break 362
%token T_l_comma 313
%token T_l_continue 363
%token T_l_defer 364
%token T_l_direct 349
%token T_l_div 317
%token T_l_div_assign 323
%token T_l_do 365
%token T_l_double_index_point 345
%token T_l_else 366
%token T_l_enum 367
%token T_l_equal 331
%token T_l_export 368
%token T_l_extern 369
%token T_l_false 370
%token T_l_for 371
%token T_l_format_string 310
%token T_l_format_string_begin 309
%token T_l_format_string_end 311
%token T_l_func 372
%token T_l_function_result 351
%token T_l_identifier 302
%token T_l_if 373
%token T_l_immut 374
%token T_l_import 375
%token T_l_index_begin 347
%token T_l_index_end 348
%token T_l_index_point 344
%token T_l_internal 376
%token T_l_inv_direct 350
%token T_l_is 377
%token T_l_lambda 378
%token T_l_land 337
%token T_l_larg 336
%token T_l_larg_or_equal 333
%token T_l_left_brackets 354
%token T_l_left_curly_braces 356
%token T_l_less 335
%token T_l_less_or_equal 334
%token T_l_let 379
%token T_l_literal_char 308
%token T_l_literal_handle 304
%token T_l_literal_integer 303
%token T_l_literal_raw_string 307
%token T_l_literal_real 305
%token T_l_literal_string 306
%token T_l_lnot 340
%token T_l_local 380
%token T_l_lor 338
%token T_l_macro 402
%token T_l_map_monad 353
%token T_l_match 381
%token T_l_mod 318
%token T_l_mod_assign 324
%token T_l_mul 316
%token T_l_mul_assign 322
%token T_l_mut 382
%token T_l_namespace 383
%token T_l_nil 384
%token T_l_not_equal 332
%token T_l_operator 385
%token T_l_or 339
%token T_l_pub 386
%token T_l_question 358
%token T_l_raw 387
%token T_l_return 388
%token T_l_right_brackets 355
%token T_l_right_curly_braces 357
%token T_l_scopeing 341
%token T_l_semicolon 312
%token T_l_static 389
%token T_l_struct 390
%token T_l_sub 315
%token T_l_sub_assign 321
%token T_l_template_using_begin 342
%token T_l_true 391
%token T_l_type 392
%token T_l_typeid 393
%token T_l_typemark 343
%token T_l_typeof 394
%token T_l_union 395
%token T_l_unknown_token 403
%token T_l_using 396
%token T_l_value_add_assign 326
%token T_l_value_assign 325
%token T_l_value_div_assign 329
%token T_l_value_mod_assign 330
%token T_l_value_mul_assign 328
%token T_l_value_sub_assign 327
%token T_l_variadic_sign 346
%token T_l_where 397
%token T_l_while 398

%%

PROGRAM_AUGMENTED
    : PROGRAM
        {
            $$ = R1(pass_direct<0>, $1);
            ctx->result = $1;
            ctx->result_valid = true;
        }
    ;

PROGRAM
    : /* empty */
        {
            $$ = ::wo::parser::make_empty_token(*ctx);
        }
    | USELESS_TOKEN
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | PARAGRAPH
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | EXPRESSION
        {
            $$ = R1(pass_repl_expression, $1);
        }
    ;

PARAGRAPH
    : SENTENCE_LIST
        {
            $$ = R1(pass_direct<0>, $1);
        }
    ;

SENTENCE_LIST
    : SENTENCE_LIST SENTENCE
        {
            $$ = R2(A_APPEND_1_0, $1, $2);
        }
    | SENTENCE
        {
            $$ = R1(pass_create_list<0>, $1);
        }
    ;

SENTENCE
    : T_l_defer BLOCKED_SENTENCE
        {
            $$ = R2(pass_defer, $1, $2);
        }
    | IMPORT_SENTENCE T_l_semicolon
        {
            $$ = R2(pass_direct<0>, $1, $2);
        }
    | DECL_ATTRIBUTE T_l_using SCOPED_LIST_NORMAL T_l_semicolon
        {
            $$ = R4(pass_using_namespace, $1, $2, $3, $4);
        }
    | DECL_TYPE SENTENCE_BLOCK_MAY_SEMICOLON
        {
            $$ = R2(pass_type_declare_namespace, $1, $2);
        }
    | DECL_ATTRIBUTE T_l_alias IDENTIFIER DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY T_l_typemark TYPE T_l_semicolon
        {
            $$ = R7(pass_alias_type_as, $1, $2, $3, $4, $5, $6, $7);
        }
    | DECL_ATTRIBUTE T_l_alias IDENTIFIER DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY T_l_assign EXPRESSION T_l_semicolon
        {
            $$ = R7(pass_alias_value_as, $1, $2, $3, $4, $5, $6, $7);
        }
    | DECL_ENUM
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | DECL_NAMESPACE
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | SENTENCE_BLOCK
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | T_l_semicolon
        {
            $$ = R1(pass_empty, $1);
        }
    | FUNC_DEFINE_WITH_NAME
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | MAY_LABELED_LOOP
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | T_l_break T_l_semicolon
        {
            $$ = R2(pass_break, $1, $2);
        }
    | T_l_continue T_l_semicolon
        {
            $$ = R2(pass_continue, $1, $2);
        }
    | T_l_break IDENTIFIER T_l_semicolon
        {
            $$ = R3(pass_break_label, $1, $2, $3);
        }
    | T_l_continue IDENTIFIER T_l_semicolon
        {
            $$ = R3(pass_continue_label, $1, $2, $3);
        }
    | IF
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | VAR_DEFINE_LET_SENTENCE T_l_semicolon
        {
            $$ = R2(pass_direct<0>, $1, $2);
        }
    | T_l_return EXPRESSION T_l_semicolon
        {
            $$ = R3(pass_return_value, $1, $2, $3);
        }
    | T_l_return T_l_semicolon
        {
            $$ = R2(pass_return, $1, $2);
        }
    | EXPRESSION T_l_semicolon
        {
            $$ = R2(pass_direct_keep_source_location<0>, $1, $2);
        }
    | DECL_UNION
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | MATCH_BLOCK
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | error T_l_semicolon
        {
            /* statement-level recovery: discard the bad statement */
            $$ = R0(pass_empty);
        }
    ;

IMPORT_SENTENCE
    : T_l_import SCOPED_LIST_NORMAL
        {
            $$ = R2(pass_import_files, $1, $2);
        }
    | T_l_export T_l_import SCOPED_LIST_NORMAL
        {
            $$ = R3(pass_import_files, $1, $2, $3);
        }
    ;

DECL_TYPE
    : DECL_ATTRIBUTE T_l_type IDENTIFIER DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY T_l_typemark ACCESS_MODIFIER_MAY_EMPTY TYPE
        {
            $$ = R7(pass_type_declare, $1, $2, $3, $4, $5, $6, $7);
        }
    ;

SENTENCE_BLOCK_MAY_SEMICOLON
    : T_l_semicolon
        {
            $$ = R1(pass_empty, $1);
        }
    | SENTENCE_BLOCK
        {
            $$ = R1(pass_direct<0>, $1);
        }
    ;

DECL_ENUM
    : DECL_ATTRIBUTE T_l_enum ACCESS_MODIFIER_MAY_EMPTY AST_TOKEN_IDENTIFER T_l_left_curly_braces ENUM_ITEMS T_l_right_curly_braces
        {
            $$ = R7(pass_enum_finalize, $1, $2, $3, $4, $5, $6, $7);
        }
    ;

ENUM_ITEMS
    : ENUM_ITEM_LIST COMMA_MAY_EMPTY
        {
            $$ = R2(pass_direct<0>, $1, $2);
        }
    ;

ENUM_ITEM_LIST
    : ENUM_ITEM
        {
            $$ = R1(pass_create_list<0>, $1);
        }
    | ENUM_ITEM_LIST T_l_comma ENUM_ITEM
        {
            $$ = R3(A_APPEND_2_0, $1, $2, $3);
        }
    ;

ENUM_ITEM
    : ACCESS_MODIFIER_MAY_EMPTY IDENTIFIER
        {
            $$ = R2(pass_enum_item_create, $1, $2);
        }
    | ACCESS_MODIFIER_MAY_EMPTY IDENTIFIER T_l_assign EXPRESSION
        {
            $$ = R4(pass_enum_item_create, $1, $2, $3, $4);
        }
    ;

DECL_NAMESPACE
    : T_l_namespace SPACE_NAME_LIST SENTENCE_BLOCK
        {
            $$ = R3(pass_namespace, $1, $2, $3);
        }
    ;

SPACE_NAME_LIST
    : SPACE_NAME
        {
            $$ = R1(pass_create_list<0>, $1);
        }
    | SPACE_NAME_LIST T_l_scopeing SPACE_NAME
        {
            $$ = R3(A_APPEND_2_0, $1, $2, $3);
        }
    ;

SPACE_NAME
    : IDENTIFIER
        {
            $$ = R1(pass_token, $1);
        }
    ;

BLOCKED_SENTENCE
    : SENTENCE
        {
            $$ = R1(pass_sentence_block<0>, $1);
        }
    ;

SENTENCE_BLOCK
    : T_l_left_curly_braces PARAGRAPH T_l_right_curly_braces
        {
            $$ = R3(pass_sentence_block<1>, $1, $2, $3);
        }
    | T_l_left_curly_braces T_l_right_curly_braces
        {
            $$ = R2(pass_empty, $1, $2);
        }
    ;

DECL_ATTRIBUTE
    : DECL_ATTRIBUTE_ITEMS
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | /* empty */
        {
            $$ = R0(pass_empty);
        }
    ;

DECL_ATTRIBUTE_ITEMS
    : DECL_ATTRIBUTE_ITEM
        {
            $$ = R1(pass_attribute, $1);
        }
    | DECL_ATTRIBUTE_ITEMS DECL_ATTRIBUTE_ITEM
        {
            $$ = R2(pass_attribute_append, $1, $2);
        }
    ;

DECL_ATTRIBUTE_ITEM
    : LIFECYCLE_MODIFER
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | EXTERNAL_MODIFER
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | ACCESS_MODIFIER
        {
            $$ = R1(pass_direct<0>, $1);
        }
    ;

LIFECYCLE_MODIFER
    : T_l_static
        {
            $$ = R1(pass_token, $1);
        }
    ;

EXTERNAL_MODIFER
    : T_l_extern
        {
            $$ = R1(pass_token, $1);
        }
    ;

ACCESS_MODIFIER
    : T_l_pub
        {
            $$ = R1(pass_token, $1);
        }
    | T_l_internal
        {
            $$ = R1(pass_token, $1);
        }
    | T_l_local
        {
            $$ = R1(pass_token, $1);
        }
    ;

WHERE_CONSTRAINT_WITH_SEMICOLON
    : WHERE_CONSTRAINT T_l_semicolon
        {
            $$ = R2(pass_direct<0>, $1, $2);
        }
    | /* empty */
        {
            $$ = R0(pass_empty);
        }
    ;

WHERE_CONSTRAINT_MAY_EMPTY
    : WHERE_CONSTRAINT
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | /* empty */
        {
            $$ = R0(pass_empty);
        }
    ;

WHERE_CONSTRAINT
    : T_l_where CONSTRAINT_LIST COMMA_MAY_EMPTY
        {
            $$ = R3(pass_build_where_constraint, $1, $2, $3);
        }
    ;

CONSTRAINT_LIST
    : EXPRESSION
        {
            $$ = R1(pass_create_list<0>, $1);
        }
    | CONSTRAINT_LIST T_l_comma EXPRESSION
        {
            $$ = R3(A_APPEND_2_0, $1, $2, $3);
        }
    ;

FUNC_DEFINE_WITH_NAME
    : DECL_ATTRIBUTE T_l_func AST_TOKEN_IDENTIFER DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY T_l_left_brackets ARGDEFINE T_l_right_brackets RETURN_TYPE_DECLEAR_MAY_EMPTY WHERE_CONSTRAINT_WITH_SEMICOLON SENTENCE_BLOCK
        {
            $$ = R10(pass_func_def_named, $1, $2, $3, $4, $5, $6, $7, $8, $9, $10);
        }
    | DECL_ATTRIBUTE T_l_func T_l_operator OVERLOADINGABLE_OPERATOR DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY T_l_left_brackets ARGDEFINE T_l_right_brackets RETURN_TYPE_DECLEAR_MAY_EMPTY WHERE_CONSTRAINT_WITH_SEMICOLON SENTENCE_BLOCK
        {
            $$ = R11(pass_func_def_oper, $1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11);
        }
    | EXTERN_FROM DECL_ATTRIBUTE T_l_func AST_TOKEN_IDENTIFER DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY T_l_left_brackets ARGDEFINE T_l_right_brackets RETURN_TYPE_DECLEAR WHERE_CONSTRAINT_MAY_EMPTY T_l_semicolon
        {
            $$ = R11(pass_func_def_extn, $1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11);
        }
    | EXTERN_FROM DECL_ATTRIBUTE T_l_func T_l_operator OVERLOADINGABLE_OPERATOR DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY T_l_left_brackets ARGDEFINE T_l_right_brackets RETURN_TYPE_DECLEAR WHERE_CONSTRAINT_MAY_EMPTY T_l_semicolon
        {
            $$ = R12(pass_func_def_extn_oper, $1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12);
        }
    ;

OVERLOADINGABLE_OPERATOR
    : T_l_add
        {
            $$ = R1(pass_token, $1);
        }
    | T_l_sub
        {
            $$ = R1(pass_token, $1);
        }
    | T_l_mul
        {
            $$ = R1(pass_token, $1);
        }
    | T_l_div
        {
            $$ = R1(pass_token, $1);
        }
    | T_l_mod
        {
            $$ = R1(pass_token, $1);
        }
    | T_l_less
        {
            $$ = R1(pass_token, $1);
        }
    | T_l_larg
        {
            $$ = R1(pass_token, $1);
        }
    | T_l_less_or_equal
        {
            $$ = R1(pass_token, $1);
        }
    | T_l_larg_or_equal
        {
            $$ = R1(pass_token, $1);
        }
    | T_l_equal
        {
            $$ = R1(pass_token, $1);
        }
    | T_l_not_equal
        {
            $$ = R1(pass_token, $1);
        }
    | T_l_land
        {
            $$ = R1(pass_token, $1);
        }
    | T_l_lor
        {
            $$ = R1(pass_token, $1);
        }
    | T_l_index_begin T_l_index_end
        {
            $$ = R2(pass_token, $1, $2);
        }
    | T_l_as
        {
            $$ = R1(pass_token, $1);
        }
    ;

EXTERN_FROM
    : T_l_extern T_l_left_brackets EXPRESSION T_l_comma EXPRESSION T_l_right_brackets
        {
            $$ = R6(pass_extern, $1, $2, $3, $4, $5, $6);
        }
    | T_l_extern T_l_left_brackets EXPRESSION T_l_right_brackets
        {
            $$ = R4(pass_extern, $1, $2, $3, $4);
        }
    ;

MAY_LABELED_LOOP
    : IDENTIFIER T_l_at LOOP
        {
            $$ = R3(pass_mark_label, $1, $2, $3);
        }
    | LOOP
        {
            $$ = R1(pass_direct<0>, $1);
        }
    ;

LOOP
    : WHILE
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | FORLOOP
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | FOREACH
        {
            $$ = R1(pass_direct<0>, $1);
        }
    ;

WHILE
    : T_l_while T_l_left_brackets EXPRESSION T_l_right_brackets BLOCKED_SENTENCE
        {
            $$ = R5(pass_while, $1, $2, $3, $4, $5);
        }
    ;

VAR_DEFINE_WITH_SEMICOLON
    : VAR_DEFINE_LET_SENTENCE T_l_semicolon
        {
            $$ = R2(pass_direct<0>, $1, $2);
        }
    ;

FORLOOP
    : T_l_for T_l_left_brackets VAR_DEFINE_WITH_SEMICOLON MAY_EMPTY_EXPRESSION T_l_semicolon MAY_EMPTY_EXPRESSION T_l_right_brackets BLOCKED_SENTENCE
        {
            $$ = R8(pass_for_defined, $1, $2, $3, $4, $5, $6, $7, $8);
        }
    | T_l_for T_l_left_brackets MAY_EMPTY_EXPRESSION T_l_semicolon MAY_EMPTY_EXPRESSION T_l_semicolon MAY_EMPTY_EXPRESSION T_l_right_brackets BLOCKED_SENTENCE
        {
            $$ = R9(pass_for_expr, $1, $2, $3, $4, $5, $6, $7, $8, $9);
        }
    ;

MAY_EMPTY_EXPRESSION
    : EXPRESSION
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | /* empty */
        {
            $$ = R0(pass_empty);
        }
    ;

FOREACH
    : T_l_for T_l_left_brackets DECL_ATTRIBUTE T_l_let DEFINE_PATTERN T_l_typemark EXPRESSION T_l_right_brackets BLOCKED_SENTENCE
        {
            $$ = R9(pass_foreach, $1, $2, $3, $4, $5, $6, $7, $8, $9);
        }
    ;

IF
    : T_l_if T_l_left_brackets EXPRESSION T_l_right_brackets BLOCKED_SENTENCE ELSE
        {
            $$ = R6(pass_if, $1, $2, $3, $4, $5, $6);
        }
    ;

ELSE
    : /* empty */ %prec LOWER_THAN_ELSE
        {
            $$ = R0(pass_empty);
        }
    | T_l_else BLOCKED_SENTENCE
        {
            $$ = R2(pass_direct<1>, $1, $2);
        }
    ;

VAR_DEFINE_LET_SENTENCE
    : DECL_ATTRIBUTE T_l_let VARDEFINES
        {
            $$ = R3(pass_variable_defines, $1, $2, $3);
        }
    ;

VARDEFINES
    : VARDEFINE
        {
            $$ = R1(pass_create_list<0>, $1);
        }
    | VARDEFINES T_l_comma VARDEFINE
        {
            $$ = R3(A_APPEND_2_0, $1, $2, $3);
        }
    ;

VARDEFINE
    : DEFINE_PATTERN_MAY_TEMPLATE T_l_assign EXPRESSION
        {
            $$ = R3(pass_variable_define_item, $1, $2, $3);
        }
    ;

DEFINE_PATTERN_MAY_TEMPLATE
    : DEFINE_PATTERN
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | DEFINE_PATTERN_WITH_TEMPLATE
        {
            $$ = R1(pass_direct<0>, $1);
        }
    ;

EXPRESSION
    : T_l_do EXPRESSION
        {
            $$ = R2(pass_do_void_cast, $1, $2);
        }
    | T_l_mut EXPRESSION
        {
            $$ = R2(pass_mark_mut, $1, $2);
        }
    | T_l_immut EXPRESSION
        {
            $$ = R2(pass_mark_immut, $1, $2);
        }
    | ASSIGNMENT
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | LOGICAL_OR
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | LOGICAL_OR T_l_question EXPRESSION T_l_or EXPRESSION
        {
            $$ = R5(pass_conditional_expression, $1, $2, $3, $4, $5);
        }
    | ARGUMENT_EXPAND
        {
            $$ = R1(pass_direct<0>, $1);
        }
    ;

ASSIGNMENT
    : ASSIGNED_PATTERN T_l_assign EXPRESSION
        {
            $$ = R3(pass_assign_operation, $1, $2, $3);
        }
    | ASSIGNED_PATTERN T_l_add_assign EXPRESSION
        {
            $$ = R3(pass_assign_operation, $1, $2, $3);
        }
    | ASSIGNED_PATTERN T_l_sub_assign EXPRESSION
        {
            $$ = R3(pass_assign_operation, $1, $2, $3);
        }
    | ASSIGNED_PATTERN T_l_mul_assign EXPRESSION
        {
            $$ = R3(pass_assign_operation, $1, $2, $3);
        }
    | ASSIGNED_PATTERN T_l_div_assign EXPRESSION
        {
            $$ = R3(pass_assign_operation, $1, $2, $3);
        }
    | ASSIGNED_PATTERN T_l_mod_assign EXPRESSION
        {
            $$ = R3(pass_assign_operation, $1, $2, $3);
        }
    | ASSIGNED_PATTERN T_l_value_assign EXPRESSION
        {
            $$ = R3(pass_assign_operation, $1, $2, $3);
        }
    | ASSIGNED_PATTERN T_l_value_add_assign EXPRESSION
        {
            $$ = R3(pass_assign_operation, $1, $2, $3);
        }
    | ASSIGNED_PATTERN T_l_value_sub_assign EXPRESSION
        {
            $$ = R3(pass_assign_operation, $1, $2, $3);
        }
    | ASSIGNED_PATTERN T_l_value_mul_assign EXPRESSION
        {
            $$ = R3(pass_assign_operation, $1, $2, $3);
        }
    | ASSIGNED_PATTERN T_l_value_div_assign EXPRESSION
        {
            $$ = R3(pass_assign_operation, $1, $2, $3);
        }
    | ASSIGNED_PATTERN T_l_value_mod_assign EXPRESSION
        {
            $$ = R3(pass_assign_operation, $1, $2, $3);
        }
    ;

ASSIGNED_PATTERN
    : LEFT
        {
            $$ = R1(pass_pattern_for_assign, $1);
        }
    ;

FACTOR
    : FUNC_DEFINE
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | LEFT
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | T_l_left_brackets EXPRESSION T_l_right_brackets
        {
            $$ = R3(pass_direct<1>, $1, $2, $3);
        }
    | UNIT
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | FUNCTION_CALL
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | FACTOR_TYPE_CASTING T_l_bind_monad CALLABLE_LEFT
        {
            $$ = R3(pass_build_bind_monad, $1, $2, $3);
        }
    | FACTOR_TYPE_CASTING T_l_bind_monad CALLABLE_RIGHT_WITH_BRACKET
        {
            $$ = R3(pass_build_bind_monad, $1, $2, $3);
        }
    | FACTOR_TYPE_CASTING T_l_bind_monad FUNC_DEFINE
        {
            $$ = R3(pass_build_bind_monad, $1, $2, $3);
        }
    | FACTOR_TYPE_CASTING T_l_map_monad CALLABLE_LEFT
        {
            $$ = R3(pass_build_map_monad, $1, $2, $3);
        }
    | FACTOR_TYPE_CASTING T_l_map_monad CALLABLE_RIGHT_WITH_BRACKET
        {
            $$ = R3(pass_build_map_monad, $1, $2, $3);
        }
    | FACTOR_TYPE_CASTING T_l_map_monad FUNC_DEFINE
        {
            $$ = R3(pass_build_map_monad, $1, $2, $3);
        }
    ;

FUNC_DEFINE
    : DECL_ATTRIBUTE T_l_func DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY T_l_left_brackets ARGDEFINE T_l_right_brackets RETURN_TYPE_DECLEAR_MAY_EMPTY WHERE_CONSTRAINT_WITH_SEMICOLON SENTENCE_BLOCK
        {
            $$ = R9(pass_func_lambda_ml, $1, $2, $3, $4, $5, $6, $7, $8, $9);
        }
    | T_l_lambda DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY ARGDEFINE T_l_assign RETURN_EXPR_BLOCK_IN_LAMBDA WHERE_DECL_FOR_LAMBDA T_l_semicolon
        {
            $$ = R7(pass_func_lambda, $1, $2, $3, $4, $5, $6, $7);
        }
    ;

WHERE_DECL_FOR_LAMBDA
    : /* empty */
        {
            $$ = R0(pass_empty);
        }
    | T_l_where VARDEFINES
        {
            $$ = R2(pass_reverse_vardef, $1, $2);
        }
    ;

RETURN_EXPR_BLOCK_IN_LAMBDA
    : RETURN_EXPR_IN_LAMBDA
        {
            $$ = R1(pass_sentence_block<0>, $1);
        }
    ;

RETURN_EXPR_IN_LAMBDA
    : EXPRESSION
        {
            $$ = R1(pass_return_lambda, $1);
        }
    ;

RETURN_TYPE_DECLEAR_MAY_EMPTY
    : /* empty */
        {
            $$ = R0(pass_empty);
        }
    | RETURN_TYPE_DECLEAR
        {
            $$ = R1(pass_direct<0>, $1);
        }
    ;

RETURN_TYPE_DECLEAR
    : T_l_function_result TYPE
        {
            $$ = R2(pass_direct_keep_source_location<1>, $1, $2);
        }
    ;

TYPE_DECLEAR
    : T_l_typemark TYPE
        {
            $$ = R2(pass_direct_keep_source_location<1>, $1, $2);
        }
    ;

TYPE
    : ORIGIN_TYPE
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | T_l_mut ORIGIN_TYPE
        {
            $$ = R2(pass_type_mutable, $1, $2);
        }
    | T_l_immut ORIGIN_TYPE
        {
            $$ = R2(pass_type_immutable, $1, $2);
        }
    ;

ORIGIN_TYPE
    : AST_TOKEN_NIL
        {
            $$ = R1(pass_type_nil, $1);
        }
    | TUPLE_TYPE_LIST T_l_function_result TYPE
        {
            $$ = R3(pass_type_func, $1, $2, $3);
        }
    | T_l_struct T_l_left_curly_braces STRUCT_MEMBER_DEFINES T_l_right_curly_braces
        {
            $$ = R4(pass_type_struct, $1, $2, $3, $4);
        }
    | STRUCTABLE_TYPE
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | TUPLE_TYPE_LIST
        {
            $$ = R1(pass_type_tuple, $1);
        }
    ;

STRUCT_MEMBER_DEFINES
    : /* empty */
        {
            $$ = R0(pass_create_list<0>);
        }
    | STRUCT_MEMBERS_LIST COMMA_MAY_EMPTY
        {
            $$ = R2(pass_direct<0>, $1, $2);
        }
    ;

STRUCT_MEMBERS_LIST
    : STRUCT_MEMBER_PAIR
        {
            $$ = R1(pass_create_list<0>, $1);
        }
    | STRUCT_MEMBERS_LIST T_l_comma STRUCT_MEMBER_PAIR
        {
            $$ = R3(A_APPEND_2_0, $1, $2, $3);
        }
    ;

STRUCT_MEMBER_PAIR
    : ACCESS_MODIFIER_MAY_EMPTY IDENTIFIER TYPE_DECLEAR
        {
            $$ = R3(pass_type_struct_field, $1, $2, $3);
        }
    ;

ACCESS_MODIFIER_MAY_EMPTY
    : ACCESS_MODIFIER
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | /* empty */
        {
            $$ = R0(pass_empty);
        }
    ;

STRUCTABLE_TYPE
    : SCOPED_IDENTIFIER_FOR_TYPE
        {
            $$ = R1(pass_type_from_identifier, $1);
        }
    | TYPEOF
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | UNDERLYING
        {
            $$ = R1(pass_direct<0>, $1);
        }
    ;

STRUCTABLE_TYPE_FOR_CONSTRUCT
    : SCOPED_IDENTIFIER_FOR_VAL
        {
            $$ = R1(pass_type_from_identifier, $1);
        }
    | TYPEOF
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | UNDERLYING
        {
            $$ = R1(pass_direct<0>, $1);
        }
    ;

TYPEOF
    : T_l_typeof T_l_left_brackets EXPRESSION T_l_right_brackets
        {
            $$ = R4(pass_typeof, $1, $2, $3, $4);
        }
    | T_l_typeof T_l_template_using_begin TYPE T_l_larg
        {
            $$ = R4(pass_direct<2>, $1, $2, $3, $4);
        }
    | T_l_typeof T_l_less TYPE T_l_larg
        {
            $$ = R4(pass_direct<2>, $1, $2, $3, $4);
        }
    ;

UNDERLYING
    : T_l_raw TYPE
        {
            $$ = R2(pass_underlying, $1, $2);
        }
    ;

TUPLE_TYPE_LIST
    : T_l_left_brackets TUPLE_TYPE_LIST_ITEMS_MAY_EMPTY T_l_right_brackets
        {
            $$ = R3(pass_direct<1>, $1, $2, $3);
        }
    ;

TUPLE_TYPE_LIST_ITEMS_MAY_EMPTY
    : TUPLE_TYPE_LIST_ITEMS
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | VARIADIC_MAY_EMPTY
        {
            $$ = R1(pass_create_list<0>, $1);
        }
    ;

TUPLE_TYPE_LIST_ITEMS
    : TYPE
        {
            $$ = R1(pass_create_list<0>, $1);
        }
    | TUPLE_TYPE_LIST_ITEMS T_l_comma TYPE
        {
            $$ = R3(A_APPEND_2_0, $1, $2, $3);
        }
    | TUPLE_TYPE_LIST_ITEMS T_l_comma VARIADIC_MAY_EMPTY
        {
            $$ = R3(A_APPEND_2_0, $1, $2, $3);
        }
    ;

VARIADIC_MAY_EMPTY
    : T_l_variadic_sign
        {
            $$ = R1(pass_token, $1);
        }
    | /* empty */
        {
            $$ = R0(pass_empty);
        }
    ;

ARGDEFINE
    : /* empty */
        {
            $$ = R0(pass_create_list<0>);
        }
    | ARGDEFINE_VAR_ITEM
        {
            $$ = R1(pass_create_list<0>, $1);
        }
    | ARGDEFINE T_l_comma ARGDEFINE_VAR_ITEM
        {
            $$ = R3(A_APPEND_2_0, $1, $2, $3);
        }
    ;

ARGDEFINE_VAR_ITEM
    : DEFINE_PATTERN TYPE_DECL_MAY_EMPTY
        {
            $$ = R2(pass_func_argument, $1, $2);
        }
    | T_l_variadic_sign
        {
            $$ = R1(pass_token, $1);
        }
    ;

TYPE_DECL_MAY_EMPTY
    : TYPE_DECLEAR
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | /* empty */
        {
            $$ = R0(pass_empty);
        }
    ;

LOGICAL_OR
    : LOGICAL_AND
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | LOGICAL_OR T_l_lor LOGICAL_AND
        {
            $$ = R3(pass_binary_operation, $1, $2, $3);
        }
    ;

LOGICAL_AND
    : EQUATION
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | LOGICAL_AND T_l_land EQUATION
        {
            $$ = R3(pass_binary_operation, $1, $2, $3);
        }
    ;

EQUATION
    : RELATION
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | EQUATION T_l_equal RELATION
        {
            $$ = R3(pass_binary_operation, $1, $2, $3);
        }
    | EQUATION T_l_not_equal RELATION
        {
            $$ = R3(pass_binary_operation, $1, $2, $3);
        }
    ;

RELATION
    : SUMMATION
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | RELATION T_l_larg SUMMATION
        {
            $$ = R3(pass_binary_operation, $1, $2, $3);
        }
    | RELATION T_l_less SUMMATION
        {
            $$ = R3(pass_binary_operation, $1, $2, $3);
        }
    | RELATION T_l_less_or_equal SUMMATION
        {
            $$ = R3(pass_binary_operation, $1, $2, $3);
        }
    | RELATION T_l_larg_or_equal SUMMATION
        {
            $$ = R3(pass_binary_operation, $1, $2, $3);
        }
    ;

SUMMATION
    : MULTIPLICATION
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | SUMMATION T_l_add MULTIPLICATION
        {
            $$ = R3(pass_binary_operation, $1, $2, $3);
        }
    | SUMMATION T_l_sub MULTIPLICATION
        {
            $$ = R3(pass_binary_operation, $1, $2, $3);
        }
    ;

MULTIPLICATION
    : SINGLE_VALUE
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | MULTIPLICATION T_l_mul SINGLE_VALUE
        {
            $$ = R3(pass_binary_operation, $1, $2, $3);
        }
    | MULTIPLICATION T_l_div SINGLE_VALUE
        {
            $$ = R3(pass_binary_operation, $1, $2, $3);
        }
    | MULTIPLICATION T_l_mod SINGLE_VALUE
        {
            $$ = R3(pass_binary_operation, $1, $2, $3);
        }
    ;

SINGLE_VALUE
    : UNARIED_FACTOR
        {
            $$ = R1(pass_direct<0>, $1);
        }
    ;

UNARIED_FACTOR
    : FACTOR_TYPE_CASTING
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | T_l_sub UNARIED_FACTOR
        {
            $$ = R2(pass_unary_operation, $1, $2);
        }
    | T_l_lnot UNARIED_FACTOR
        {
            $$ = R2(pass_unary_operation, $1, $2);
        }
    | INV_FUNCTION_CALL
        {
            $$ = R1(pass_direct<0>, $1);
        }
    ;

FACTOR_TYPE_CASTING
    : FACTOR_TYPE_CASTING AS_TYPE
        {
            $$ = R2(pass_cast_type, $1, $2);
        }
    | FACTOR_TYPE_CASTING IS_TYPE
        {
            $$ = R2(pass_check_type_is, $1, $2);
        }
    | FACTOR_TYPE_CASTING TYPE_DECLEAR
        {
            $$ = R2(pass_check_type_assert, $1, $2);
        }
    | FACTOR
        {
            $$ = R1(pass_direct<0>, $1);
        }
    ;

AS_TYPE
    : T_l_as TYPE
        {
            $$ = R2(pass_direct_keep_source_location<1>, $1, $2);
        }
    ;

IS_TYPE
    : T_l_is TYPE
        {
            $$ = R2(pass_direct_keep_source_location<1>, $1, $2);
        }
    ;

ARGUMENT_EXPAND
    : FACTOR_TYPE_CASTING T_l_variadic_sign
        {
            $$ = R2(pass_expand_arguments, $1, $2);
        }
    ;

UNIT
    : T_l_variadic_sign
        {
            $$ = R1(pass_variadic_arguments_pack, $1);
        }
    | T_l_literal_integer
        {
            $$ = R1(pass_literal, $1);
        }
    | T_l_literal_real
        {
            $$ = R1(pass_literal, $1);
        }
    | T_l_literal_string
        {
            $$ = R1(pass_literal, $1);
        }
    | T_l_literal_raw_string
        {
            $$ = R1(pass_literal, $1);
        }
    | LITERAL_CHAR
        {
            $$ = R1(pass_literal_char, $1);
        }
    | T_l_literal_handle
        {
            $$ = R1(pass_literal, $1);
        }
    | T_l_nil
        {
            $$ = R1(pass_literal, $1);
        }
    | T_l_true
        {
            $$ = R1(pass_literal, $1);
        }
    | T_l_false
        {
            $$ = R1(pass_literal, $1);
        }
    | T_l_typeid T_l_template_using_begin TYPE T_l_larg
        {
            $$ = R4(pass_typeid, $1, $2, $3, $4);
        }
    | T_l_typeid T_l_less TYPE T_l_larg
        {
            $$ = R4(pass_typeid, $1, $2, $3, $4);
        }
    | T_l_typeid T_l_left_brackets EXPRESSION T_l_right_brackets
        {
            $$ = R4(pass_typeid_with_expr, $1, $2, $3, $4);
        }
    | FORMAT_STRING
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | CONSTANT_MAP
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | CONSTANT_ARRAY
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | CONSTANT_TUPLE
        {
            $$ = R1(pass_tuple_instance, $1);
        }
    | STRUCT_INSTANCE_BEGIN T_l_left_curly_braces STRUCT_MEMBER_INITS T_l_right_curly_braces
        {
            $$ = R4(pass_struct_instance, $1, $2, $3, $4);
        }
    ;

LITERAL_CHAR
    : T_l_literal_char
        {
            $$ = R1(pass_token, $1);
        }
    ;

FORMAT_STRING
    : LITERAL_FORMAT_STRING_BEGIN FORMAT_STRING_LIST LITERAL_FORMAT_STRING_END
        {
            $$ = R3(pass_format_finish, $1, $2, $3);
        }
    ;

FORMAT_STRING_LIST
    : EXPRESSION
        {
            $$ = R1(pass_format_cast_string, $1);
        }
    | FORMAT_STRING_LIST LITERAL_FORMAT_STRING EXPRESSION
        {
            $$ = R3(pass_format_connect, $1, $2, $3);
        }
    ;

LITERAL_FORMAT_STRING_BEGIN
    : T_l_format_string_begin
        {
            $$ = R1(pass_token, $1);
        }
    ;

LITERAL_FORMAT_STRING
    : T_l_format_string
        {
            $$ = R1(pass_token, $1);
        }
    ;

LITERAL_FORMAT_STRING_END
    : T_l_format_string_end
        {
            $$ = R1(pass_token, $1);
        }
    ;

CONSTANT_ARRAY
    : T_l_index_begin CONSTANT_ARRAY_ITEMS T_l_index_end
        {
            $$ = R3(pass_array_instance, $1, $2, $3);
        }
    | T_l_index_begin CONSTANT_ARRAY_ITEMS T_l_index_end T_l_mut
        {
            $$ = R4(pass_vec_instance, $1, $2, $3, $4);
        }
    ;

CONSTANT_ARRAY_ITEMS
    : COMMA_EXPR
        {
            $$ = R1(pass_direct<0>, $1);
        }
    ;

CONSTANT_TUPLE
    : T_l_left_brackets COMMA_MAY_EMPTY T_l_right_brackets
        {
            $$ = R3(pass_create_list<1>, $1, $2, $3);
        }
    | T_l_left_brackets EXPRESSION T_l_comma COMMA_EXPR T_l_right_brackets
        {
            $$ = R5(A_APPEND_1_3, $1, $2, $3, $4, $5);
        }
    ;

CONSTANT_MAP
    : T_l_left_curly_braces CONSTANT_MAP_PAIRS T_l_right_curly_braces
        {
            $$ = R3(pass_dict_instance, $1, $2, $3);
        }
    | T_l_left_curly_braces CONSTANT_MAP_PAIRS T_l_right_curly_braces T_l_mut
        {
            $$ = R4(pass_map_instance, $1, $2, $3, $4);
        }
    ;

CONSTANT_MAP_PAIRS
    : CONSTANT_MAP_PAIR
        {
            $$ = R1(pass_create_list<0>, $1);
        }
    | CONSTANT_MAP_PAIRS T_l_comma CONSTANT_MAP_PAIR
        {
            $$ = R3(A_APPEND_2_0, $1, $2, $3);
        }
    ;

CONSTANT_MAP_PAIR
    : /* empty */ %prec LOWER_THAN_RIGHT_CURLY
        {
            $$ = R0(pass_empty);
        }
    | CONSTANT_ARRAY T_l_assign EXPRESSION
        {
            $$ = R3(pass_dict_field_init_pair, $1, $2, $3);
        }
    ;

CALLABLE_LEFT
    : SCOPED_IDENTIFIER_FOR_VAL
        {
            $$ = R1(pass_variable, $1);
        }
    ;

CALLABLE_RIGHT_WITH_BRACKET
    : T_l_left_brackets EXPRESSION T_l_right_brackets
        {
            $$ = R3(pass_direct<1>, $1, $2, $3);
        }
    ;

SCOPED_IDENTIFIER_FOR_VAL
    : SCOPED_LIST_TYPEOF MAY_EMPTY_LEFT_TEMPLATE_ITEM
        {
            $$ = R2(pass_build_identifier_typeof, $1, $2);
        }
    | SCOPED_LIST_NORMAL MAY_EMPTY_LEFT_TEMPLATE_ITEM
        {
            $$ = R2(pass_build_identifier_normal, $1, $2);
        }
    | SCOPED_LIST_GLOBAL MAY_EMPTY_LEFT_TEMPLATE_ITEM
        {
            $$ = R2(pass_build_identifier_global, $1, $2);
        }
    ;

SCOPED_IDENTIFIER_FOR_TYPE
    : SCOPED_LIST_TYPEOF MAY_EMPTY_TEMPLATE_ITEM
        {
            $$ = R2(pass_build_identifier_typeof, $1, $2);
        }
    | SCOPED_LIST_NORMAL MAY_EMPTY_TEMPLATE_ITEM
        {
            $$ = R2(pass_build_identifier_normal, $1, $2);
        }
    | SCOPED_LIST_GLOBAL MAY_EMPTY_TEMPLATE_ITEM
        {
            $$ = R2(pass_build_identifier_global, $1, $2);
        }
    ;

SCOPED_LIST_TYPEOF
    : TYPEOF SCOPING_LIST
        {
            $$ = R2(A_APPEND_0_1, $1, $2);
        }
    ;

SCOPED_LIST_NORMAL
    : AST_TOKEN_IDENTIFER SCOPING_LIST
        {
            $$ = R2(A_APPEND_0_1, $1, $2);
        }
    | AST_TOKEN_IDENTIFER
        {
            $$ = R1(pass_create_list<0>, $1);
        }
    ;

SCOPED_LIST_GLOBAL
    : SCOPING_LIST
        {
            $$ = R1(pass_direct<0>, $1);
        }
    ;

SCOPING_LIST
    : T_l_scopeing AST_TOKEN_IDENTIFER
        {
            $$ = R2(pass_create_list<1>, $1, $2);
        }
    | T_l_scopeing AST_TOKEN_IDENTIFER SCOPING_LIST
        {
            $$ = R3(A_APPEND_1_2, $1, $2, $3);
        }
    ;

LEFT
    : CALLABLE_LEFT
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | FACTOR_TYPE_CASTING T_l_index_begin EXPRESSION T_l_index_end
        {
            $$ = R4(pass_index_operation_regular, $1, $2, $3, $4);
        }
    | FACTOR_TYPE_CASTING T_l_index_point INDEX_POINT_TARGET
        {
            $$ = R3(pass_index_operation_member, $1, $2, $3);
        }
    ;

INDEX_POINT_TARGET
    : IDENTIFIER
        {
            $$ = R1(pass_token, $1);
        }
    | T_l_literal_integer
        {
            $$ = R1(pass_token, $1);
        }
    ;

DIRECT_CALLABLE_TARGET
    : CALLABLE_LEFT
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | CALLABLE_RIGHT_WITH_BRACKET
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | FUNC_DEFINE
        {
            $$ = R1(pass_direct<0>, $1);
        }
    ;

DIRECT_CALL_FIRST_ARG
    : FACTOR_TYPE_CASTING
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | ARGUMENT_EXPAND
        {
            $$ = R1(pass_direct<0>, $1);
        }
    ;

ARGUMENT_LISTS
    : T_l_left_brackets COMMA_EXPR T_l_right_brackets
        {
            $$ = R3(pass_direct<1>, $1, $2, $3);
        }
    ;

ARGUMENT_LISTS_MAY_EMPTY
    : ARGUMENT_LISTS
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | /* empty */ %prec LOWER_THAN_LEFT_BRACKETS
        {
            $$ = R0(pass_create_list<0>);
        }
    ;

FUNCTION_CALL
    : FACTOR ARGUMENT_LISTS
        {
            $$ = R2(pass_normal_function_call, $1, $2);
        }
    | DIRECT_CALLABLE_VALUE ARGUMENT_LISTS_MAY_EMPTY
        {
            $$ = R2(pass_directly_function_call_append_arguments, $1, $2);
        }
    ;

DIRECT_CALLABLE_VALUE
    : DIRECT_CALL_FIRST_ARG T_l_direct DIRECT_CALLABLE_TARGET
        {
            $$ = R3(pass_directly_function_call, $1, $2, $3);
        }
    ;

INV_DIRECT_CALL_FIRST_ARG
    : SINGLE_VALUE
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | ARGUMENT_EXPAND
        {
            $$ = R1(pass_direct<0>, $1);
        }
    ;

INV_FUNCTION_CALL
    : FUNCTION_CALL T_l_inv_direct INV_DIRECT_CALL_FIRST_ARG
        {
            $$ = R3(pass_inverse_function_call, $1, $2, $3);
        }
    | DIRECT_CALLABLE_TARGET T_l_inv_direct INV_DIRECT_CALL_FIRST_ARG
        {
            $$ = R3(pass_inverse_function_call, $1, $2, $3);
        }
    ;

COMMA_EXPR
    : COMMA_EXPR_ITEMS COMMA_MAY_EMPTY
        {
            $$ = R2(pass_direct<0>, $1, $2);
        }
    | COMMA_MAY_EMPTY
        {
            $$ = R1(pass_create_list<0>, $1);
        }
    ;

COMMA_EXPR_ITEMS
    : EXPRESSION
        {
            $$ = R1(pass_create_list<0>, $1);
        }
    | COMMA_EXPR_ITEMS T_l_comma EXPRESSION
        {
            $$ = R3(A_APPEND_2_0, $1, $2, $3);
        }
    ;

COMMA_MAY_EMPTY
    : T_l_comma
        {
            $$ = R1(pass_empty, $1);
        }
    | /* empty */
        {
            $$ = R0(pass_empty);
        }
    ;

MAY_EMPTY_TEMPLATE_ITEM
    : TYPE_TEMPLATE_ITEM
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | /* empty */ %prec LOWER_THAN_LESS
        {
            $$ = R0(pass_empty);
        }
    ;

MAY_EMPTY_LEFT_TEMPLATE_ITEM
    : /* empty */
        {
            $$ = R0(pass_empty);
        }
    | LEFT_TEMPLATE_ITEM
        {
            $$ = R1(pass_direct<0>, $1);
        }
    ;

LEFT_TEMPLATE_ITEM
    : T_l_template_using_begin TEMPLATE_ARGUMENT_LIST T_l_larg
        {
            $$ = R3(pass_direct<1>, $1, $2, $3);
        }
    ;

TYPE_TEMPLATE_ITEM
    : T_l_less TEMPLATE_ARGUMENT_LIST T_l_larg
        {
            $$ = R3(pass_direct<1>, $1, $2, $3);
        }
    | LEFT_TEMPLATE_ITEM
        {
            $$ = R1(pass_direct<0>, $1);
        }
    ;

TEMPLATE_ARGUMENT_LIST
    : TEMPLATE_ARGUMENT_ITEM
        {
            $$ = R1(pass_create_list<0>, $1);
        }
    | TEMPLATE_ARGUMENT_LIST T_l_comma TEMPLATE_ARGUMENT_ITEM
        {
            $$ = R3(A_APPEND_2_0, $1, $2, $3);
        }
    ;

TEMPLATE_ARGUMENT_ITEM
    : TYPE
        {
            $$ = R1(pass_create_template_argument, $1);
        }
    | T_l_left_curly_braces EXPRESSION T_l_right_curly_braces
        {
            $$ = R3(pass_create_template_argument, $1, $2, $3);
        }
    ;

DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY
    : /* empty */
        {
            $$ = R0(pass_empty);
        }
    | DEFINE_TEMPLATE_PARAM_ITEM
        {
            $$ = R1(pass_direct<0>, $1);
        }
    ;

DEFINE_TEMPLATE_PARAM_ITEM
    : T_l_less DEFINE_TEMPLATE_PARAM_LIST T_l_larg
        {
            $$ = R3(pass_direct<1>, $1, $2, $3);
        }
    ;

DEFINE_TEMPLATE_PARAM_LIST
    : DEFINE_TEMPLATE_PARAM
        {
            $$ = R1(pass_create_list<0>, $1);
        }
    | DEFINE_TEMPLATE_PARAM_LIST T_l_comma DEFINE_TEMPLATE_PARAM
        {
            $$ = R3(A_APPEND_2_0, $1, $2, $3);
        }
    ;

DEFINE_TEMPLATE_PARAM
    : IDENTIFIER
        {
            $$ = R1(pass_create_template_param, $1);
        }
    | IDENTIFIER T_l_typemark TYPE
        {
            $$ = R3(pass_create_template_param, $1, $2, $3);
        }
    ;

DECL_UNION
    : DECL_ATTRIBUTE T_l_union ACCESS_MODIFIER_MAY_EMPTY AST_TOKEN_IDENTIFER DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY T_l_left_curly_braces UNION_ITEMS T_l_right_curly_braces
        {
            $$ = R8(pass_union_declare, $1, $2, $3, $4, $5, $6, $7, $8);
        }
    ;

UNION_ITEMS
    : UNION_ITEM_LIST COMMA_MAY_EMPTY
        {
            $$ = R2(pass_direct<0>, $1, $2);
        }
    ;

UNION_ITEM_LIST
    : UNION_ITEM
        {
            $$ = R1(pass_create_list<0>, $1);
        }
    | UNION_ITEM_LIST T_l_comma UNION_ITEM
        {
            $$ = R3(A_APPEND_2_0, $1, $2, $3);
        }
    ;

UNION_ITEM
    : ACCESS_MODIFIER_MAY_EMPTY IDENTIFIER
        {
            $$ = R2(pass_union_item, $1, $2);
        }
    | ACCESS_MODIFIER_MAY_EMPTY IDENTIFIER T_l_left_brackets TYPE T_l_right_brackets
        {
            $$ = R5(pass_union_item_constructor, $1, $2, $3, $4, $5);
        }
    ;

MATCH_BLOCK
    : T_l_match T_l_left_brackets EXPRESSION T_l_right_brackets T_l_left_curly_braces MATCH_CASES T_l_right_curly_braces
        {
            $$ = R7(pass_match, $1, $2, $3, $4, $5, $6, $7);
        }
    ;

MATCH_CASES
    : MATCH_CASE
        {
            $$ = R1(pass_create_list<0>, $1);
        }
    | MATCH_CASES MATCH_CASE
        {
            $$ = R2(A_APPEND_1_0, $1, $2);
        }
    ;

MATCH_CASE
    : PATTERN_UNION_CASE T_l_question BLOCKED_SENTENCE
        {
            $$ = R3(pass_match_union_case, $1, $2, $3);
        }
    ;

PATTERN_UNION_CASE
    : IDENTIFIER
        {
            $$ = R1(pass_union_pattern_identifier_or_takeplace, $1);
        }
    | IDENTIFIER T_l_left_brackets DEFINE_PATTERN T_l_right_brackets
        {
            $$ = R4(pass_union_pattern_contain_element, $1, $2, $3, $4);
        }
    ;

STRUCT_INSTANCE_BEGIN
    : STRUCTABLE_TYPE_FOR_CONSTRUCT
        {
            $$ = R1(pass_direct<0>, $1);
        }
    | T_l_struct
        {
            $$ = R1(pass_empty, $1);
        }
    ;

STRUCT_MEMBER_INITS
    : STRUCT_MEMBER_INITS_EMPTY
        {
            $$ = R1(pass_create_list<0>, $1);
        }
    | STRUCT_MEMBERS_INIT_LIST COMMA_MAY_EMPTY
        {
            $$ = R2(pass_direct<0>, $1, $2);
        }
    ;

STRUCT_MEMBER_INITS_EMPTY
    : /* empty */
        {
            $$ = R0(pass_empty);
        }
    | T_l_comma
        {
            $$ = R1(pass_empty, $1);
        }
    ;

STRUCT_MEMBERS_INIT_LIST
    : STRUCT_MEMBER_INIT_ITEM
        {
            $$ = R1(pass_create_list<0>, $1);
        }
    | STRUCT_MEMBERS_INIT_LIST T_l_comma STRUCT_MEMBER_INIT_ITEM
        {
            $$ = R3(A_APPEND_2_0, $1, $2, $3);
        }
    ;

STRUCT_MEMBER_INIT_ITEM
    : IDENTIFIER T_l_assign EXPRESSION
        {
            $$ = R3(pass_struct_member_init_pair, $1, $2, $3);
        }
    ;

DEFINE_PATTERN
    : IDENTIFIER
        {
            $$ = R1(pass_pattern_identifier_or_takepace, $1);
        }
    | T_l_mut IDENTIFIER
        {
            $$ = R2(pass_pattern_mut_identifier_or_takepace, $1, $2);
        }
    | T_l_left_brackets DEFINE_PATTERN_LIST T_l_right_brackets
        {
            $$ = R3(pass_pattern_tuple, $1, $2, $3);
        }
    | T_l_left_brackets COMMA_MAY_EMPTY T_l_right_brackets
        {
            $$ = R3(pass_pattern_tuple, $1, $2, $3);
        }
    ;

DEFINE_PATTERN_WITH_TEMPLATE
    : IDENTIFIER DEFINE_TEMPLATE_PARAM_ITEM
        {
            $$ = R2(pass_pattern_identifier_or_takepace_with_template, $1, $2);
        }
    | T_l_mut IDENTIFIER DEFINE_TEMPLATE_PARAM_ITEM
        {
            $$ = R3(pass_pattern_mut_identifier_or_takepace_with_template, $1, $2, $3);
        }
    ;

DEFINE_PATTERN_LIST
    : DEFINE_PATTERN_ITEMS COMMA_MAY_EMPTY
        {
            $$ = R2(pass_direct<0>, $1, $2);
        }
    ;

DEFINE_PATTERN_ITEMS
    : DEFINE_PATTERN
        {
            $$ = R1(pass_create_list<0>, $1);
        }
    | DEFINE_PATTERN_ITEMS T_l_comma DEFINE_PATTERN
        {
            $$ = R3(A_APPEND_2_0, $1, $2, $3);
        }
    ;

AST_TOKEN_IDENTIFER
    : IDENTIFIER
        {
            $$ = R1(pass_token, $1);
        }
    ;

AST_TOKEN_NIL
    : T_l_nil
        {
            $$ = R1(pass_token, $1);
        }
    ;

IDENTIFIER
    : T_l_identifier
        {
            $$ = R1(pass_direct<0>, $1);
        }
    ;

USELESS_TOKEN
    : T_l_double_index_point
        {
            $$ = R1(pass_useless_token, $1);
        }
    | T_l_unknown_token
        {
            $$ = R1(pass_useless_token, $1);
        }
    | T_l_macro
        {
            $$ = R1(pass_token, $1);

            ctx->lex.record_diagnose(
                ::wo::lexer::msglevel_t::error,
                $1.begin.row_no,
                $1.begin.col_no,
                $1.end.row_no,
                $1.end.col_no,
                ctx->lex.get_source_path(),
                ::wo::diagnose::parser::err_unknown_macro_named{
                    $1.tok_identifier->c_str() });
        }
    ;

%%
