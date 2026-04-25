#pragma once
// Woolang Header
//
// Here will have woolang c api;
//
#define WO_VERSION WO_VERSION_WRAP(1, 15, 0, 0)

#ifndef WO_MSVC_RC_INCLUDE

#include "woort.h"

#ifdef __cplusplus
#include <cstdint>
#include <cstddef>
#include <clocale>
#define WO_FORCE_CAPI \
    extern "C"        \
    {
#define WO_FORCE_CAPI_END }
#define WO_DECLARE_ALIGNAS(VAL) alignas(VAL)
#else
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <locale.h>
#include <stdalign.h>
#define WO_FORCE_CAPI     /* nothing */
#define WO_FORCE_CAPI_END /* nothing */
#define WO_DECLARE_ALIGNAS(VAL) _Alignas(VAL)
#endif

#ifdef _WIN32
#define WO_IMPORT __declspec(dllimport)
#define WO_EXPORT __declspec(dllexport)
#else
#define WO_IMPORT extern
#define WO_EXPORT extern
#endif

#ifdef WO_IMPL
#define WO_STRICTLY_BOOL
#define WO_IMPORT_OR_EXPORT WO_EXPORT
#else
#define WO_IMPORT_OR_EXPORT WO_IMPORT
#endif

#ifdef WO_STATIC_LIB
#define WO_API
#else
#define WO_API WO_IMPORT_OR_EXPORT
#endif

WO_FORCE_CAPI

#define WO_STRUCT_TAKE_PLACE(BYTECOUNT) uint8_t _take_palce_[BYTECOUNT]

// Return WO_FALSE to abort vm.
typedef bool (*wo_fail_handler_t)(
    /* OPTIONAL */ woort_VMRuntime* vm_may_null,
    const char* src_file,
    uint32_t lineno,
    const char* functionname,
    uint32_t rterrcode,
    const char* reason);

typedef void* wo_dylib_handle_t;

typedef enum _wo_dylib_unload_method_t
{
    WO_DYLIB_NONE = 0,

    // WO_DYLIB_UNREF: Reduces the number of references to the specified library,
    //  and when the last reference is released, removes the library instance from
    //  the reference table.
    WO_DYLIB_UNREF = 1 << 0,

    // WO_DYLIB_BURY: Removes the library from the lookup table without changing
    //  the reference count of the library.
    WO_DYLIB_BURY = 1 << 1,

    WO_DYLIB_UNREF_AND_BURY = WO_DYLIB_UNREF | WO_DYLIB_BURY,

} wo_dylib_unload_method_t;

typedef enum _wo_inform_style_t
{
    WO_DEFAULT = 0,

    WO_NOTHING = 1,
    WO_NEED_COLOR = 2,

} wo_inform_style_t;

typedef struct _wo_extern_lib_func_pair
{
    const char* m_name;
    void* m_func_addr;
} wo_extern_lib_func_t;

typedef struct _wo_virtual_file* wo_virtual_file_t;
typedef struct _wo_virtual_file_iter* wo_virtual_file_iter_t;

typedef struct _wo_pin_value* wo_pin_value;
typedef struct _wo_weak_ref* wo_weak_ref;

#define WO_EXTERN_LIB_FUNC_END \
    wo_extern_lib_func_t { nullptr, nullptr }
#define wo_fail(ERRID, ...) abort()
#define wo_execute_fail(VM, ERRID, REASON) abort()

WO_API const char* wo_commit_sha(void);
WO_API const char* wo_compile_date(void);
WO_API const char* wo_version(void);
WO_API uint64_t wo_version_int(void);

WO_API const char* wo_locale_name(void);

WO_API void wo_init(int argc, char** argv);
#define wo_init(argc, argv)                    \
    do                                         \
    {                                          \
        wo_init(argc, argv);                   \
        setlocale(LC_CTYPE, wo_locale_name()); \
    } while (0)
WO_API void wo_finish(void (*do_after_shutdown)(void*), void* custom_data);

WO_API bool wo_virtual_binary(
    const char* filepath,
    const void* data,
    size_t len,
    bool enable_modify);
WO_API bool wo_virtual_source(
    const char* filepath,
    const char* data,
    bool enable_modify);

WO_API wo_virtual_file_t wo_open_virtual_file(const char* filepath);
WO_API const char* wo_virtual_file_path(wo_virtual_file_t file);
WO_API const void* wo_virtual_file_data(wo_virtual_file_t file, size_t* len);
WO_API void wo_close_virtual_file(wo_virtual_file_t file);

WO_API wo_virtual_file_iter_t wo_open_virtual_file_iter(void);
WO_API const char* /* may null */ wo_next_virtual_file_iter(wo_virtual_file_iter_t iter);
WO_API void wo_close_virtual_file_iter(wo_virtual_file_iter_t iter);

WO_API bool wo_remove_virtual_file(const char* filepath);

// Load woolang source/file/binary and get compiled CodeEnv without a VM.
// Returns woort_CodeEnv* on success (caller must woort_CodeEnv_drop() it).
// On failure, returns NULL. If out_errors != NULL, *out_errors is set to
// a wo_CompileErrors object that can be iterated (caller must free it).

typedef struct _wo_CompileErrorInfo
{
    const char* m_file_name;
    const char* m_message;
    size_t      m_begin_row;
    size_t      m_begin_col;
    size_t      m_end_row;
    size_t      m_end_col;
    int         m_is_error; // 1 = error, 0 = information
} wo_CompileErrorInfo;

typedef struct _wo_CompileErrors wo_CompileErrors;

#define WO_DEFAULT_ENTRY "@entry"

WO_API /* OPTIONAL */ woort_CodeEnv* wo_load_source(
    woort_U8CString virtual_src_path,
    woort_U8CString src,
    /* OPTIONAL */ wo_CompileErrors** out_errors);

WO_API /* OPTIONAL */ woort_CodeEnv* wo_load_file(
    woort_U8CString virtual_src_path,
    /* OPTIONAL */ wo_CompileErrors** out_errors);

WO_API /* OPTIONAL */ woort_CodeEnv* wo_load_binary(
    woort_U8CString virtual_src_path,
    const void* buffer,
    size_t length,
    /* OPTIONAL */ wo_CompileErrors** out_errors);

// Iterate compile errors. Returns pointer to internal wo_CompileErrorInfo
// (valid until next call or wo_compile_errors_free). Returns NULL when exhausted.
WO_API wo_CompileErrorInfo* wo_compile_errors_next(wo_CompileErrors* errors);

// Free the compile errors iterator.
WO_API void wo_compile_errors_free(wo_CompileErrors* errors);

// Get a formatted compile error string (rich output with source snippets and colors).
// style: WO_DEFAULT, WO_NOTHING, or WO_NEED_COLOR.
// Returns a thread-local C string, valid until the next call.
WO_API const char* wo_get_compile_error(
    wo_CompileErrors* errors,
    wo_inform_style_t style);

WO_API char32_t wo_str_get_char(const char* str, size_t index);
WO_API char32_t wo_strn_get_char(const char* str, size_t size, size_t index);

WO_API const wchar_t* wo_str_to_wstr(const char* str);
WO_API const wchar_t* wo_strn_to_wstr(const char* str, size_t size);
WO_API const char* wo_wstr_to_str(const wchar_t* str);
WO_API const char* wo_wstrn_to_str(const wchar_t* str, size_t size);

WO_API const char16_t* wo_str_to_u16str(const char* str);
WO_API const char16_t* wo_strn_to_u16str(const char* str, size_t size);
WO_API const char* wo_u16str_to_str(const char16_t* str);
WO_API const char* wo_u16strn_to_str(const char16_t* str, size_t size);

WO_API const char32_t* wo_str_to_u32str(const char* str);
WO_API const char32_t* wo_strn_to_u32str(const char* str, size_t size);
WO_API const char* wo_u32str_to_str(const char32_t* str);
WO_API const char* wo_u32strn_to_str(const char32_t* str, size_t size);

WO_API int64_t wo_crc64_u8(uint8_t byte, int64_t crc);
WO_API int64_t wo_crc64_str(const char* text);
WO_API int64_t wo_crc64_file(const char* filepath);

#if defined(WO_IMPL)
#define WO_NEED_ERROR_CODES 1
#define WO_NEED_ANSI_CONTROL 1
#define WO_NEED_LSP_API 1
#define WO_NEED_OPCODE_API 1
#endif

#if defined(WO_NEED_LSP_API)
// Following API named with wo_lsp_... will be used for getting meta-data by language-service.

typedef enum _wo_lsp_error_level
{
    WO_LSP_ERROR,
    WO_LSP_INFORMATION,

} wo_lsp_error_level;

// LSPv2 API
typedef struct _wo_lspv2_location
{
    const char* m_file_name;
    size_t m_begin_location[2]; // An array stores row & col
    size_t m_end_location[2];   // An array stores row & col
} wo_lspv2_location;

typedef struct _wo_lspv2_source_meta wo_lspv2_source_meta;
typedef struct _wo_lspv2_scope wo_lspv2_scope;
typedef struct _wo_lspv2_scope_iter wo_lspv2_scope_iter;
typedef struct _wo_lspv2_symbol wo_lspv2_symbol;
typedef struct _wo_lspv2_symbol_iter wo_lspv2_symbol_iter;

typedef struct _wo_lspv2_scope_info
{
    const char* m_name; // null if not namespace
    bool m_has_location;
    wo_lspv2_location m_location;
} wo_lspv2_scope_info;

typedef enum _wo_lspv2_symbol_kind
{
    WO_LSPV2_SYMBOL_VARIBALE,
    WO_LSPV2_SYMBOL_TYPE,
    WO_LSPV2_SYMBOL_ALIAS,

} wo_lspv2_symbol_kind;

typedef struct _wo_lspv2_type wo_lspv2_type;
typedef struct _wo_lspv2_constant wo_lspv2_constant;

typedef struct _wo_lspv2_constant_info
{
    const char* m_expr;

} wo_lspv2_constant_info;

typedef enum _wo_lspv2_template_param_kind
{
    WO_LSPV2_TEMPLATE_PARAM_TYPE,
    WO_LSPV2_TEMPLATE_PARAM_CONSTANT,

}wo_lspv2_template_param_kind;

typedef struct _wo_lspv2_template_param
{
    wo_lspv2_template_param_kind m_kind;
    const char* m_name;

}wo_lspv2_template_param;

typedef struct _wo_lspv2_template_argument
{
    wo_lspv2_template_param_kind m_kind;
    wo_lspv2_type* m_type;
    wo_lspv2_constant* m_constant_may_null;

}wo_lspv2_template_argument;

typedef struct _wo_lspv2_symbol_info
{
    wo_lspv2_symbol_kind m_type;
    const char* m_name;
    size_t m_template_params_count;
    const _wo_lspv2_template_param* m_template_params;
    bool m_has_location;
    wo_lspv2_location m_location;
} wo_lspv2_symbol_info;

typedef struct _wo_lspv2_error_iter wo_lspv2_error_iter;
typedef struct _wo_lspv2_error_info
{
    wo_lsp_error_level m_level;
    size_t m_depth;
    const char* m_describe;
    wo_lspv2_location m_location;

} wo_lspv2_error_info;

typedef struct _wo_lspv2_expr_collection_iter wo_lspv2_expr_collection_iter;
typedef struct _wo_lspv2_expr_collection wo_lspv2_expr_collection;
typedef struct _wo_lspv2_expr_collection_info
{
    const char* m_file_name;
} wo_lspv2_expr_collection_info;

typedef struct _wo_lspv2_type_info
{
    const char* m_name;
    wo_lspv2_symbol* m_type_symbol;
    size_t m_template_arguments_count;
    const wo_lspv2_template_argument* m_template_arguments;
} wo_lspv2_type_info;

typedef struct _wo_lspv2_expr_iter wo_lspv2_expr_iter;
typedef struct _wo_lspv2_expr wo_lspv2_expr;
typedef struct _wo_lspv2_expr_info
{
    wo_lspv2_type* m_type;
    wo_lspv2_location m_location;
    wo_lspv2_symbol* m_symbol_may_null;
    bool m_is_value_expr;       // false if type.
    wo_lspv2_constant* m_const_value_may_null; // null if not const or is type.
    size_t m_template_arguments_count;
    const wo_lspv2_template_argument* m_template_arguments; // null if not template instance.
} wo_lspv2_expr_info;

typedef struct _wo_lspv2_type_struct_info
{
    size_t m_member_count;
    const char** m_member_names;
    wo_lspv2_type** m_member_types;

} wo_lspv2_type_struct_info;

typedef struct _wo_lspv2_macro wo_lspv2_macro;
typedef struct _wo_lspv2_macro_iter wo_lspv2_macro_iter;
typedef struct _wo_lspv2_macro_info
{
    const char* m_name;
    wo_lspv2_location m_location;
} wo_lspv2_macro_info;

typedef enum _wo_lspv2_lexer_token
{
    WO_LSPV2_TOKEN_EOF = -1,
    WO_LSPV2_TOKEN_ERROR = 0,
    WO_LSPV2_TOKEN_EMPTY,                // [empty]
    WO_LSPV2_TOKEN_IDENTIFIER,           // identifier.
    WO_LSPV2_TOKEN_LITERAL_INTEGER,      // 1 233 0x123456 0b1101001 032
    WO_LSPV2_TOKEN_LITERAL_HANDLE,       // 0L 256L 0xFFL
    WO_LSPV2_TOKEN_LITERAL_REAL,         // 0.2  0.
    WO_LSPV2_TOKEN_LITERAL_STRING,       // "helloworld"
    WO_LSPV2_TOKEN_LITERAL_RAW_STRING,   // @ raw_string @
    WO_LSPV2_TOKEN_LITERAL_CHAR,         // 'x'
    WO_LSPV2_TOKEN_FORMAT_STRING_BEGIN,  // F"..{
    WO_LSPV2_TOKEN_FORMAT_STRING,        // }..{ 
    WO_LSPV2_TOKEN_FORMAT_STRING_END,    // }.."
    WO_LSPV2_TOKEN_SEMICOLON,            // ;
    WO_LSPV2_TOKEN_COMMA,                // ,
    WO_LSPV2_TOKEN_ADD,                  // +
    WO_LSPV2_TOKEN_SUB,                  // - 
    WO_LSPV2_TOKEN_MUL,                  // * 
    WO_LSPV2_TOKEN_DIV,                  // / 
    WO_LSPV2_TOKEN_MOD,                  // % 
    WO_LSPV2_TOKEN_ASSIGN,               // =
    WO_LSPV2_TOKEN_ADD_ASSIGN,           // +=
    WO_LSPV2_TOKEN_SUB_ASSIGN,           // -= 
    WO_LSPV2_TOKEN_MUL_ASSIGN,           // *=
    WO_LSPV2_TOKEN_DIV_ASSIGN,           // /= 
    WO_LSPV2_TOKEN_MOD_ASSIGN,           // %= 
    WO_LSPV2_TOKEN_VALUE_ASSIGN,         // :=
    WO_LSPV2_TOKEN_VALUE_ADD_ASSIGN,     // +:=
    WO_LSPV2_TOKEN_VALUE_SUB_ASSIGN,     // -:= 
    WO_LSPV2_TOKEN_VALUE_MUL_ASSIGN,     // *:=
    WO_LSPV2_TOKEN_VALUE_DIV_ASSIGN,     // /:= 
    WO_LSPV2_TOKEN_VALUE_MOD_ASSIGN,     // %:= 
    WO_LSPV2_TOKEN_EQUAL,                // ==
    WO_LSPV2_TOKEN_NOT_EQUAL,            // !=
    WO_LSPV2_TOKEN_LARG_OR_EQUAL,        // >=
    WO_LSPV2_TOKEN_LESS_OR_EQUAL,        // <=
    WO_LSPV2_TOKEN_LESS,                 // <
    WO_LSPV2_TOKEN_LARG,                 // >
    WO_LSPV2_TOKEN_LAND,                 // &&
    WO_LSPV2_TOKEN_LOR,                  // ||
    WO_LSPV2_TOKEN_OR,                   // |
    WO_LSPV2_TOKEN_LNOT,                 // !
    WO_LSPV2_TOKEN_SCOPEING,             // ::
    WO_LSPV2_TOKEN_TEMPLATE_USING_BEGIN, // :<
    WO_LSPV2_TOKEN_TYPECAST,             // :
    WO_LSPV2_TOKEN_INDEX_POINT,          // .
    WO_LSPV2_TOKEN_DOUBLE_INDEX_POINT,   // .. [Reserved]
    WO_LSPV2_TOKEN_VARIADIC_SIGN,        // ...
    WO_LSPV2_TOKEN_INDEX_BEGIN,          // '['
    WO_LSPV2_TOKEN_INDEX_END,            // ']'
    WO_LSPV2_TOKEN_DIRECT,               // '->'
    WO_LSPV2_TOKEN_INV_DIRECT,           // '<|'
    WO_LSPV2_TOKEN_FUNCTION_RESULT,      // '=>'
    WO_LSPV2_TOKEN_BIND_MONAD,           // '=>>'
    WO_LSPV2_TOKEN_MAP_MONAD,            // '->>'
    WO_LSPV2_TOKEN_LEFT_BRACKETS,        // (
    WO_LSPV2_TOKEN_RIGHT_BRACKETS,       // )
    WO_LSPV2_TOKEN_LEFT_CURLY_BRACES,    // {
    WO_LSPV2_TOKEN_RIGHT_CURLY_BRACES,   // }
    WO_LSPV2_TOKEN_QUESTION,             // ?
    WO_LSPV2_TOKEN_IMPORT,
    WO_LSPV2_TOKEN_EXPORT,
    WO_LSPV2_TOKEN_NIL,
    WO_LSPV2_TOKEN_TRUE,
    WO_LSPV2_TOKEN_FALSE,
    WO_LSPV2_TOKEN_WHILE,
    WO_LSPV2_TOKEN_IF,
    WO_LSPV2_TOKEN_ELSE,
    WO_LSPV2_TOKEN_NAMESPACE,
    WO_LSPV2_TOKEN_FOR,
    WO_LSPV2_TOKEN_EXTERN,
    WO_LSPV2_TOKEN_LET,
    WO_LSPV2_TOKEN_MUT,
    WO_LSPV2_TOKEN_FUNC,
    WO_LSPV2_TOKEN_RETURN,
    WO_LSPV2_TOKEN_USING,
    WO_LSPV2_TOKEN_ALIAS,
    WO_LSPV2_TOKEN_ENUM,
    WO_LSPV2_TOKEN_AS,
    WO_LSPV2_TOKEN_IS,
    WO_LSPV2_TOKEN_TYPEOF,
    WO_LSPV2_TOKEN_PRIVATE,
    WO_LSPV2_TOKEN_PUBLIC,
    WO_LSPV2_TOKEN_PROTECTED,
    WO_LSPV2_TOKEN_STATIC,
    WO_LSPV2_TOKEN_BREAK,
    WO_LSPV2_TOKEN_CONTINUE,
    WO_LSPV2_TOKEN_LAMBDA,
    WO_LSPV2_TOKEN_AT,
    WO_LSPV2_TOKEN_DO,
    WO_LSPV2_TOKEN_WHERE,
    WO_LSPV2_TOKEN_OPERATOR,
    WO_LSPV2_TOKEN_UNION,
    WO_LSPV2_TOKEN_MATCH,
    WO_LSPV2_TOKEN_STRUCT,
    WO_LSPV2_TOKEN_IMMUT,
    WO_LSPV2_TOKEN_TYPEID,
    WO_LSPV2_TOKEN_DEFER,
    WO_LSPV2_TOKEN_MACRO,
    WO_LSPV2_TOKEN_LINE_COMMENT,
    WO_LSPV2_TOKEN_BLOCK_COMMENT,
    WO_LSPV2_TOKEN_SHEBANG_COMMENT,
    WO_LSPV2_TOKEN_UNKNOWN_TOKEN,

}wo_lspv2_lexer_token;

typedef struct _wo_lspv2_lexer wo_lspv2_lexer;
typedef struct _wo_lspv2_token_info
{
    wo_lspv2_lexer_token m_token;

    const void* m_token_serial;
    size_t m_token_length;
    wo_lspv2_location m_location;

} wo_lspv2_token_info;

WO_API size_t wo_lspv2_sub_version(void);

WO_API wo_lspv2_source_meta* wo_lspv2_compile_to_meta(
    const char* virtual_src_path,
    const char* src);
WO_API void wo_lspv2_meta_free(wo_lspv2_source_meta* meta);

WO_API wo_lspv2_error_iter* /* null if not exist */wo_lspv2_compile_err_iter(
    wo_lspv2_source_meta* meta);
WO_API wo_lspv2_error_info* /* null if end */wo_lspv2_compile_err_next(
    wo_lspv2_error_iter* iter);
WO_API void wo_lspv2_err_info_free(wo_lspv2_error_info* info);

WO_API const char* /* Thread local */ wo_lspv2_token_info_enstring(
    const void* p, size_t len);

// Macro API
WO_API wo_lspv2_macro_iter* /* null if grammar failed */
wo_lspv2_meta_macro_iter(wo_lspv2_source_meta* meta);
WO_API wo_lspv2_macro* /* null if end */ wo_lspv2_macro_next(wo_lspv2_macro_iter* iter);
WO_API wo_lspv2_macro_info* wo_lspv2_macro_get_info(wo_lspv2_macro* macro);
WO_API void wo_lspv2_macro_info_free(wo_lspv2_macro_info* info);

// Scope API
WO_API wo_lspv2_scope* /* null if grammar failed */
wo_lspv2_meta_get_global_scope(wo_lspv2_source_meta* meta);
WO_API wo_lspv2_scope_iter* wo_lspv2_scope_sub_scope_iter(wo_lspv2_scope* scope);
WO_API wo_lspv2_scope* /* null if end */ wo_lspv2_scope_sub_scope_next(wo_lspv2_scope_iter* iter);
WO_API wo_lspv2_scope_info* wo_lspv2_scope_get_info(wo_lspv2_scope* scope);
WO_API void wo_lspv2_scope_info_free(wo_lspv2_scope_info* info);

// Symbol API
WO_API wo_lspv2_symbol_iter* wo_lspv2_scope_symbol_iter(wo_lspv2_scope* scope);
WO_API wo_lspv2_symbol* /* null if end */ wo_lspv2_scope_symbol_next(wo_lspv2_symbol_iter* iter);
WO_API wo_lspv2_symbol_info* wo_lspv2_symbol_get_info(wo_lspv2_symbol* symbol);
WO_API void wo_lspv2_symbol_info_free(wo_lspv2_symbol_info* info);

// Expr API
WO_API wo_lspv2_expr_collection_iter* wo_lspv2_meta_expr_collection_iter(
    wo_lspv2_source_meta* meta);
WO_API wo_lspv2_expr_collection* /* null if end */ wo_lspv2_expr_collection_next(
    wo_lspv2_expr_collection_iter* iter);
WO_API void wo_lspv2_expr_collection_free(wo_lspv2_expr_collection* collection);
WO_API wo_lspv2_expr_collection_info* wo_lspv2_expr_collection_get_info(
    wo_lspv2_expr_collection* collection);
WO_API void wo_lspv2_expr_collection_info_free(wo_lspv2_expr_collection_info* collection);
WO_API wo_lspv2_expr_iter* /* null if not found */ wo_lspv2_expr_collection_get_by_range(
    wo_lspv2_expr_collection* collection,
    size_t begin_row,
    size_t begin_col,
    size_t end_row,
    size_t end_col);
WO_API wo_lspv2_expr* /* null if end */ wo_lspv2_expr_next(wo_lspv2_expr_iter* iter);
WO_API wo_lspv2_expr_info* wo_lspv2_expr_get_info(wo_lspv2_expr* expr);
WO_API void wo_lspv2_expr_info_free(wo_lspv2_expr_info*);

// Type API
WO_API wo_lspv2_type_info* wo_lspv2_type_get_info(
    wo_lspv2_type* type, wo_lspv2_source_meta* meta);
WO_API void wo_lspv2_type_info_free(wo_lspv2_type_info* info);
WO_API wo_lspv2_type_struct_info* /* null if not struct */ wo_lspv2_type_get_struct_info(
    wo_lspv2_type* type, wo_lspv2_source_meta* meta);
WO_API void wo_lspv2_type_struct_info_free(wo_lspv2_type_struct_info* info);

// Constant API
WO_API wo_lspv2_constant_info* wo_lspv2_constant_get_info(
    wo_lspv2_constant* constant, wo_lspv2_source_meta* meta);
WO_API void wo_lspv2_constant_info_free(wo_lspv2_constant_info* info);

// Lexer API
WO_API wo_lspv2_lexer* wo_lspv2_lexer_create(const char* src);
WO_API void wo_lspv2_lexer_free(wo_lspv2_lexer* lexer);
WO_API wo_lspv2_token_info* wo_lspv2_lexer_peek(wo_lspv2_lexer* lexer);
WO_API void wo_lspv2_lexer_consume(wo_lspv2_lexer* lexer);
WO_API void wo_lspv2_token_info_free(wo_lspv2_token_info* info);
#endif

#if defined(WO_NEED_ERROR_CODES)

#   define WO_FAIL_USER_PANIC 0xD001
#   define WO_FAIL_NOT_SUPPORT 0xD002
#   define WO_FAIL_TYPE_FAIL 0xD003
#   define WO_FAIL_ACCESS_NIL 0xD004
#   define WO_FAIL_INDEX_FAIL 0xD005
#   define WO_FAIL_CALL_FAIL 0xD006
#   define WO_FAIL_BAD_LIB 0xD007
#   define WO_FAIL_UNEXPECTED 0xD008
#   define WO_FAIL_STACKOVERFLOW 0xD009
#   define WO_FAIL_DEBUGGEE_FAIL 0xD00A
#   define WO_FAIL_EXECUTE_FAIL 0xD00B
#   define WO_FAIL_BAD_FORMAT 0xD00C
#   define WO_FAIL_GC_GUARD_VIOLATION 0xD00D

#endif

#if defined(WO_NEED_ANSI_CONTROL)

// You can use this macro to specify ANSI_XXX as a wide character string.
// NOTE: After use, you MUST re-define as nothing.
#   define ANSI_ESC "\033["
#   define ANSI_END "m"
#   define ANSI_RST ANSI_ESC "0m"
#   define ANSI_HIL ANSI_ESC "1m"
#   define ANSI_FAINT ANSI_ESC "2m"
#   define ANSI_ITALIC ANSI_ESC "3m"
#   define ANSI_UNDERLNE ANSI_ESC "4m"
#   define ANSI_NUNDERLNE ANSI_ESC "24m"
#   define ANSI_SLOW_BLINK ANSI_ESC "5m"
#   define ANSI_FAST_BLINK ANSI_ESC "6m"
#   define ANSI_INV ANSI_ESC "7m"
#   define ANSI_FADE ANSI_ESC "8m"
#   define ANSI_BLK ANSI_ESC "30m"
#   define ANSI_GRY ANSI_ESC "1;30m"
#   define ANSI_RED ANSI_ESC "31m"
#   define ANSI_HIR ANSI_ESC "1;31m"
#   define ANSI_GRE ANSI_ESC "32m"
#   define ANSI_HIG ANSI_ESC "1;32m"
#   define ANSI_YEL ANSI_ESC "33m"
#   define ANSI_HIY ANSI_ESC "1;33m"
#   define ANSI_BLU ANSI_ESC "34m"
#   define ANSI_HIB ANSI_ESC "1;34m"
#   define ANSI_MAG ANSI_ESC "35m"
#   define ANSI_HIM ANSI_ESC "1;35m"
#   define ANSI_CLY ANSI_ESC "36m"
#   define ANSI_HIC ANSI_ESC "1;36m"
#   define ANSI_WHI ANSI_ESC "37m"
#   define ANSI_HIW ANSI_ESC "1;37m"
#   define ANSI_BBLK ANSI_ESC "40m"
#   define ANSI_BGRY ANSI_ESC "1;40m"
#   define ANSI_BRED ANSI_ESC "41m"
#   define ANSI_BHIR ANSI_ESC "1;41m"
#   define ANSI_BGRE ANSI_ESC "42m"
#   define ANSI_BHIG ANSI_ESC "1;42m"
#   define ANSI_BYEL ANSI_ESC "43m"
#   define ANSI_BHIY ANSI_ESC "1;43m"
#   define ANSI_BBLU ANSI_ESC "44m"
#   define ANSI_BHIB ANSI_ESC "1;44m"
#   define ANSI_BMAG ANSI_ESC "45m"
#   define ANSI_BHIM ANSI_ESC "1;45m"
#   define ANSI_BCLY ANSI_ESC "46m"
#   define ANSI_BHIC ANSI_ESC "1;46m"
#   define ANSI_BWHI ANSI_ESC "47m"
#   define ANSI_BHIW ANSI_ESC "1;47m"
#endif

WO_FORCE_CAPI_END
#undef WO_API

#ifdef _WIN32
#ifdef __cplusplus
#define WO_API extern "C" WO_EXPORT
#else
#define WO_API WO_EXPORT
#endif
#else
#ifdef __cplusplus
#define WO_API extern "C"
#else
#define WO_API WO_EXPORT
#endif
#endif

#endif // End of WO_MSVC_RC_INCLUDE
// NOTE: must contain a empty line following this line, .rc file will need it.
