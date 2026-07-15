#pragma once

/**
 * @brief Woolang C API
 *
 * High-level public API for the Woolang scripting language.
 * Provides compiler, runtime integration,
 * LSP service, and utility functions.
 * Depends on woort.h for low-level runtime types.
 */

/** @brief Woolang version encoded as (major, minor, patch, tweak). */
#define WO_VERSION WO_VERSION_WRAP(1, 15, 3, 6)

#ifndef WO_MSVC_RC_INCLUDE

/** @brief Low-level runtime API (VM, IR compiler, bytecode). */
#include "woort.h"

#ifdef __cplusplus
#include <cstdint>
#include <cstddef>
#include <clocale>
/** @brief Begin a C-linkage block for C++ compatibility. */
#define WO_FORCE_CAPI \
    extern "C"        \
    {
/** @brief End a C-linkage block for C++ compatibility. */
#define WO_FORCE_CAPI_END }
/** @brief Cross-platform alignment specifier. */
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

/** @brief DLL import symbol (Win32) or extern (other platforms). */
#ifdef _WIN32
#define WO_IMPORT __declspec(dllimport)
#define WO_EXPORT __declspec(dllexport)
#else
#define WO_IMPORT extern
#define WO_EXPORT extern
#endif

/** @brief Select import or export based on whether woolang is being built or consumed. */
#ifdef WO_IMPL
#define WO_IMPORT_OR_EXPORT WO_EXPORT
#else
#define WO_IMPORT_OR_EXPORT WO_IMPORT
#endif

/** @brief Public API visibility: empty for static lib, import/export for shared lib. */
#ifdef WO_STATIC_LIB
#define WO_API
#else
#define WO_API WO_IMPORT_OR_EXPORT
#endif

WO_FORCE_CAPI

/**
 * @brief Output formatting style for diagnostic messages.
 */
typedef enum _wo_inform_style_t
{
    /** @brief Use ANSI Console control encode to make text colorful. */
    WO_COLORFUL = 0,

    /** @brief Just get plaim text. */
    WO_PLAIM,

} wo_inform_style_t;

/* ========== Version & Locale API ========== */

/**
 * @brief Get the Git commit SHA from which woolang was built.
 * @return A null-terminated C string containing the commit SHA.
 */
WO_API const char* wo_commit_sha(void);

/**
 * @brief Get the compile date/time string.
 * @return A null-terminated C string containing the compile timestamp.
 */
WO_API const char* wo_compile_date(void);

/**
 * @brief Get the woolang version string.
 * @return A version string in the form "major.minor.patch.tweak" (e.g. "1.15.0.0").
 */
WO_API const char* wo_version(void);

/**
 * @brief Get the woolang version as a packed 64-bit integer.
 * @return The version encoded as a 64-bit integer.
 */
WO_API uint64_t wo_version_int(void);


/* ========== Lifecycle API ========== */

/**
 * @brief Initialize the woolang runtime.
 *
 * Must be called once before any other woolang API functions.
 * The macro form additionally calls setlocale(LC_CTYPE, wo_locale_name()).
 *
 * @param argc  Argument count (as passed to main).
 * @param argv  Argument vector (as passed to main).
 */
WO_API void wo_init(int argc, char** argv);
#define wo_init(argc, argv)                             \
    do                                                  \
    {                                                   \
        wo_init(argc, argv);                            \
        setlocale(LC_CTYPE, woort_env_locale_name());   \
    } while (0)

/**
 * @brief Shut down the woolang runtime and run a cleanup callback.
 *
 * @param do_after_shutdown  Optional callback invoked after shutdown is complete.
 * @param custom_data        User data passed to the callback.
 */
WO_API void wo_finish(void (*do_after_shutdown)(void*), void* custom_data);

/* ========== Compiler Options Help API ========== */

/**
 * @brief Print the available woolang compiler command-line options to stdout.
 *
 * Lists every --woolang-* option recognized by wo_init(), with accepted
 * values and defaults.
 */
WO_API void wo_print_compiler_help(void);

/* ========== Compile API ========== */

/**
 * @brief A single compile error or informational message.
 */
typedef struct _wo_CompileErrorInfo
{
    const char* m_file_name;   /**< @brief Source file name. */
    const char* m_message;     /**< @brief Error or information message text. */
    size_t      m_begin_row;   /**< @brief Start row (1-based). */
    size_t      m_begin_col;   /**< @brief Start column (1-based). */
    size_t      m_end_row;     /**< @brief End row (1-based). */
    size_t      m_end_col;     /**< @brief End column (1-based). */
    int         m_is_error;    /**< @brief 1 = error, 0 = information. */
    size_t      m_layer;       /**< @brief Depth. */
} wo_CompileErrorInfo;

/** @brief Opaque iterator for enumerating compile errors. */
typedef struct _wo_CompileErrors wo_CompileErrors;

/**
 * @brief Compile Woolang source code and produce a CodeEnv without a VM.
 *
 * @param virtual_src_path  Virtual path used for error reporting and module resolution.
 * @param src               Source code string to compile.
 * @param out_errors        Optional pointer to receive a compile errors iterator (must be freed).
 * @return A woort_CodeEnv* on success, or NULL on failure.
 * @note Caller must woort_CodeEnv_drop() the returned CodeEnv when done.
 */
WO_API /* OPTIONAL */ woort_CodeEnv* wo_load_source(
    woort_U8CString virtual_src_path,
    woort_U8CString src,
    /* OPTIONAL */ wo_CompileErrors** out_errors);

/**
 * @brief Load and compile a source file and produce a CodeEnv without a VM.
 *
 * @param virtual_src_path  Path to the source file to load.
 * @param out_errors        Optional pointer to receive compile errors (must be freed).
 * @return A woort_CodeEnv* on success, or NULL on failure.
 * @note Caller must woort_CodeEnv_drop() the returned CodeEnv when done.
 */
WO_API /* OPTIONAL */ woort_CodeEnv* wo_load_file(
    woort_U8CString virtual_src_path,
    /* OPTIONAL */ wo_CompileErrors** out_errors);

/**
 * @brief Load a precompiled binary and produce a CodeEnv without a VM.
 *
 * @param virtual_src_path  Virtual path used for error reporting.
 * @param buffer            Pointer to the compiled binary data.
 * @param length            Length of the binary data.
 * @param out_errors        Optional pointer to receive compile errors (must be freed).
 * @return A woort_CodeEnv* on success, or NULL on failure.
 * @note Caller must woort_CodeEnv_drop() the returned CodeEnv when done.
 */
WO_API /* OPTIONAL */ woort_CodeEnv* wo_load_binary(
    woort_U8CString virtual_src_path,
    const void* buffer,
    size_t length,
    /* OPTIONAL */ wo_CompileErrors** out_errors);

/**
 * @brief Advance the compile errors iterator and return the next error.
 *
 * @param errors  The compile errors iterator.
 * @return A pointer to the next wo_CompileErrorInfo, or NULL when exhausted.
 * @note The returned pointer is valid until the next call to this function or
 *       wo_compile_errors_free().
 */
WO_API wo_CompileErrorInfo* wo_compile_errors_next(wo_CompileErrors* errors);

/**
 * @brief Free a compile errors iterator.
 * @param errors  The compile errors iterator to free.
 */
WO_API void wo_compile_errors_free(wo_CompileErrors* errors);

/**
 * @brief Get a formatted compile error string with source snippets and colors.
 *
 * @param errors  The compile errors iterator.
 * @param style   Output style: WO_DEFAULT, WO_NOTHING, or WO_NEED_COLOR.
 * @return A thread-local C string, valid until the next call to this function.
 */
WO_API const char* wo_get_compile_error(
    wo_CompileErrors* errors,
    wo_inform_style_t style);

/* ========== REPL API ========== */

/**
 * @brief Opaque handle to a REPL session.
 *
 * A REPL session maintains persistent compiler state (symbol table, type
 * table) and a persistent VM across multiple evaluations, so that bindings
 * declared in one evaluation are visible in subsequent ones.
 */
typedef struct _wo_ReplSession wo_ReplSession;

/** @brief Result of a single REPL evaluation. */
typedef enum _wo_repl_result
{
    WO_REPL_OK = 0,               /**< @brief Evaluation succeeded. */
    WO_REPL_COMPILE_ERROR,        /**< @brief Compilation failed (see out_errors). */
    WO_REPL_INCOMPLETE_INPUT,     /**< @brief Input is syntactically incomplete; read more. */
    WO_REPL_RUNTIME_ERROR,        /**< @brief Runtime panic occurred. */
    WO_REPL_OUT_OF_MEMORY,        /**< @brief Allocation failure. */

} wo_repl_result;

/**
 * @brief Create a new REPL session.
 *
 * Allocates a persistent LangContext (with builtins and stdlib registered)
 * and a persistent VM. The caller must destroy the session with
 * wo_repl_destroy().
 *
 * @return A new REPL session handle, or NULL on failure.
 */
WO_API /* OPTIONAL */ wo_ReplSession* wo_repl_create(void);

/**
 * @brief Destroy a REPL session and release all resources.
 *
 * Drops the persistent VM, all CodeEnvs produced during the session, and
 * the persistent compiler state.
 *
 * @param session  The session to destroy (may be NULL).
 */
WO_API void wo_repl_destroy(/* OPTIONAL */ wo_ReplSession* session);

/**
 * @brief Evaluate a source snippet in the REPL session.
 *
 * Compiles and executes the given source code in the context of the
 * session. Bindings declared at the top level (let, func, type, etc.)
 * are accumulated and become visible in subsequent evaluations.
 *
 * @param session     The REPL session.
 * @param src         Null-terminated source code string.
 * @param out_errors  Optional pointer to receive compile errors (must be freed).
 * @return The evaluation result.
 */
WO_API wo_repl_result wo_repl_eval(
    wo_ReplSession* session,
    woort_U8CString src,
    /* OPTIONAL */ wo_CompileErrors** out_errors);

/* ========== CRC64 API ========== */

/**
 * @brief Accumulate a single byte into a CRC-64 checksum.
 * @param byte  The byte to incorporate.
 * @param crc   The current CRC-64 value (use 0 to start).
 * @return The updated CRC-64 checksum.
 */
WO_API uint64_t wo_crc64_u8(uint8_t byte, uint64_t crc);

/**
 * @brief Compute the CRC-64 checksum of a null-terminated string.
 * @param text  The input string.
 * @return The CRC-64 checksum.
 */
WO_API uint64_t wo_crc64_str(const char* text);

/**
 * @brief Compute the CRC-64 checksum of a file's contents.
 * @param file  The file.
 * @return The CRC-64 checksum, or 0 on error.
 */
WO_API uint64_t wo_crc64_file(woort_VFile* file);

/**
 * @brief Compute the CRC-64 checksum of a file's contents.
 * @param filepath  Path to the file.
 * @return The CRC-64 checksum, or 0 on error.
 */
WO_API uint64_t wo_crc64_file_from_path(const char* filepath);

#if defined(WO_IMPL)
#define WO_NEED_ANSI_CONTROL 1
#define WO_NEED_LSP_API 1
#define WO_NEED_OPCODE_API 1
#endif

/* ========== LSP (Language Server Protocol) API ========== */

#if defined(WO_NEED_LSP_API) || defined(WO_NEED_LSP_LEXER_API)
/** @name LSP Lexer API */
/**@{*/

/** @brief Source location spanning a range in a file. */
typedef struct _wo_lspv2_location
{
    const char* m_file_name;         /**< @brief Source file path. */
    size_t m_begin_location[2];      /**< @brief Start position [row, col] (0-based). */
    size_t m_end_location[2];        /**< @brief End position [row, col] (0-based). */
} wo_lspv2_location;

/**
 * @brief Lexer token types for the Woolang tokenizer (LSP use).
 */
typedef enum _wo_lspv2_lexer_token
{
    WO_LSPV2_TOKEN_EOF = -1,                /**< @brief End of input. */
    WO_LSPV2_TOKEN_ERROR = 0,               /**< @brief Lexer error. */
    WO_LSPV2_TOKEN_EMPTY,                   /**< @brief Empty token. */
    WO_LSPV2_TOKEN_IDENTIFIER,              /**< @brief Identifier. */
    WO_LSPV2_TOKEN_LITERAL_INTEGER,         /**< @brief Integer literal (e.g. 1, 0x1234). */
    WO_LSPV2_TOKEN_LITERAL_HANDLE,          /**< @brief Handle literal (e.g. 0L, 0xFFL). */
    WO_LSPV2_TOKEN_LITERAL_REAL,            /**< @brief Real literal (e.g. 0.2). */
    WO_LSPV2_TOKEN_LITERAL_STRING,          /**< @brief String literal ("hello"). */
    WO_LSPV2_TOKEN_LITERAL_RAW_STRING,      /**< @brief Raw string literal (@ raw @). */
    WO_LSPV2_TOKEN_LITERAL_CHAR,            /**< @brief Character literal ('x'). */
    WO_LSPV2_TOKEN_FORMAT_STRING_BEGIN,     /**< @brief Format string begin (F"..{). */
    WO_LSPV2_TOKEN_FORMAT_STRING,           /**< @brief Format string middle (}..{). */
    WO_LSPV2_TOKEN_FORMAT_STRING_END,       /**< @brief Format string end (}.."). */
    WO_LSPV2_TOKEN_SEMICOLON,               /**< @brief ; */
    WO_LSPV2_TOKEN_COMMA,                   /**< @brief , */
    WO_LSPV2_TOKEN_ADD,                     /**< @brief + */
    WO_LSPV2_TOKEN_SUB,                     /**< @brief - */
    WO_LSPV2_TOKEN_MUL,                     /**< @brief * */
    WO_LSPV2_TOKEN_DIV,                     /**< @brief / */
    WO_LSPV2_TOKEN_MOD,                     /**< @brief % */
    WO_LSPV2_TOKEN_ASSIGN,                  /**< @brief = */
    WO_LSPV2_TOKEN_ADD_ASSIGN,              /**< @brief += */
    WO_LSPV2_TOKEN_SUB_ASSIGN,              /**< @brief -= */
    WO_LSPV2_TOKEN_MUL_ASSIGN,              /**< @brief *= */
    WO_LSPV2_TOKEN_DIV_ASSIGN,              /**< @brief /= */
    WO_LSPV2_TOKEN_MOD_ASSIGN,              /**< @brief %= */
    WO_LSPV2_TOKEN_VALUE_ASSIGN,            /**< @brief := */
    WO_LSPV2_TOKEN_VALUE_ADD_ASSIGN,        /**< @brief +:= */
    WO_LSPV2_TOKEN_VALUE_SUB_ASSIGN,        /**< @brief -:= */
    WO_LSPV2_TOKEN_VALUE_MUL_ASSIGN,        /**< @brief *:= */
    WO_LSPV2_TOKEN_VALUE_DIV_ASSIGN,        /**< @brief /:= */
    WO_LSPV2_TOKEN_VALUE_MOD_ASSIGN,        /**< @brief %:= */
    WO_LSPV2_TOKEN_EQUAL,                   /**< @brief == */
    WO_LSPV2_TOKEN_NOT_EQUAL,               /**< @brief != */
    WO_LSPV2_TOKEN_LARG_OR_EQUAL,           /**< @brief >= */
    WO_LSPV2_TOKEN_LESS_OR_EQUAL,           /**< @brief <= */
    WO_LSPV2_TOKEN_LESS,                    /**< @brief < */
    WO_LSPV2_TOKEN_LARG,                    /**< @brief > */
    WO_LSPV2_TOKEN_LAND,                    /**< @brief && */
    WO_LSPV2_TOKEN_LOR,                     /**< @brief || */
    WO_LSPV2_TOKEN_OR,                      /**< @brief | */
    WO_LSPV2_TOKEN_LNOT,                    /**< @brief ! */
    WO_LSPV2_TOKEN_SCOPEING,                /**< @brief :: */
    WO_LSPV2_TOKEN_TEMPLATE_USING_BEGIN,    /**< @brief :< */
    WO_LSPV2_TOKEN_TYPECAST,                /**< @brief : */
    WO_LSPV2_TOKEN_INDEX_POINT,             /**< @brief . */
    WO_LSPV2_TOKEN_DOUBLE_INDEX_POINT,      /**< @brief .. [Reserved] */
    WO_LSPV2_TOKEN_VARIADIC_SIGN,           /**< @brief ... */
    WO_LSPV2_TOKEN_INDEX_BEGIN,             /**< @brief '[' */
    WO_LSPV2_TOKEN_INDEX_END,               /**< @brief ']' */
    WO_LSPV2_TOKEN_DIRECT,                  /**< @brief -> */
    WO_LSPV2_TOKEN_INV_DIRECT,              /**< @brief <| */
    WO_LSPV2_TOKEN_FUNCTION_RESULT,         /**< @brief => */
    WO_LSPV2_TOKEN_BIND_MONAD,              /**< @brief =>> */
    WO_LSPV2_TOKEN_MAP_MONAD,               /**< @brief ->> */
    WO_LSPV2_TOKEN_LEFT_BRACKETS,           /**< @brief ( */
    WO_LSPV2_TOKEN_RIGHT_BRACKETS,          /**< @brief ) */
    WO_LSPV2_TOKEN_LEFT_CURLY_BRACES,       /**< @brief { */
    WO_LSPV2_TOKEN_RIGHT_CURLY_BRACES,      /**< @brief } */
    WO_LSPV2_TOKEN_QUESTION,                /**< @brief ? */

    /* Keywords */
    WO_LSPV2_TOKEN_IMPORT,                  /**< @brief import */
    WO_LSPV2_TOKEN_EXPORT,                  /**< @brief export */
    WO_LSPV2_TOKEN_NIL,                     /**< @brief nil */
    WO_LSPV2_TOKEN_TRUE,                    /**< @brief true */
    WO_LSPV2_TOKEN_FALSE,                   /**< @brief false */
    WO_LSPV2_TOKEN_WHILE,                   /**< @brief while */
    WO_LSPV2_TOKEN_IF,                      /**< @brief if */
    WO_LSPV2_TOKEN_ELSE,                    /**< @brief else */
    WO_LSPV2_TOKEN_NAMESPACE,               /**< @brief namespace */
    WO_LSPV2_TOKEN_FOR,                     /**< @brief for */
    WO_LSPV2_TOKEN_EXTERN,                  /**< @brief extern */
    WO_LSPV2_TOKEN_LET,                     /**< @brief let */
    WO_LSPV2_TOKEN_MUT,                     /**< @brief mut */
    WO_LSPV2_TOKEN_FUNC,                    /**< @brief func */
    WO_LSPV2_TOKEN_RETURN,                  /**< @brief return */
    WO_LSPV2_TOKEN_USING,                   /**< @brief using */
    WO_LSPV2_TOKEN_ALIAS,                   /**< @brief alias */
    WO_LSPV2_TOKEN_ENUM,                    /**< @brief enum */
    WO_LSPV2_TOKEN_AS,                      /**< @brief as */
    WO_LSPV2_TOKEN_IS,                      /**< @brief is */
    WO_LSPV2_TOKEN_TYPEOF,                  /**< @brief typeof */
    WO_LSPV2_TOKEN_PRIVATE,                 /**< @brief private */
    WO_LSPV2_TOKEN_PUBLIC,                  /**< @brief public */
    WO_LSPV2_TOKEN_PROTECTED,               /**< @brief protected */
    WO_LSPV2_TOKEN_STATIC,                  /**< @brief static */
    WO_LSPV2_TOKEN_BREAK,                   /**< @brief break */
    WO_LSPV2_TOKEN_CONTINUE,                /**< @brief continue */
    WO_LSPV2_TOKEN_LAMBDA,                  /**< @brief lambda */
    WO_LSPV2_TOKEN_AT,                      /**< @brief @ */
    WO_LSPV2_TOKEN_DO,                      /**< @brief do */
    WO_LSPV2_TOKEN_WHERE,                   /**< @brief where */
    WO_LSPV2_TOKEN_OPERATOR,                /**< @brief operator */
    WO_LSPV2_TOKEN_UNION,                   /**< @brief union */
    WO_LSPV2_TOKEN_MATCH,                   /**< @brief match */
    WO_LSPV2_TOKEN_STRUCT,                  /**< @brief struct */
    WO_LSPV2_TOKEN_IMMUT,                   /**< @brief immut */
    WO_LSPV2_TOKEN_TYPEID,                  /**< @brief typeid */
    WO_LSPV2_TOKEN_DEFER,                   /**< @brief defer */
    WO_LSPV2_TOKEN_MACRO,                   /**< @brief macro */

    /* Comments */
    WO_LSPV2_TOKEN_LINE_COMMENT,            /**< @brief Line comment (//). */
    WO_LSPV2_TOKEN_BLOCK_COMMENT,           /**< @brief Block comment. */
    WO_LSPV2_TOKEN_SHEBANG_COMMENT,         /**< @brief Shebang comment (#!). */
    WO_LSPV2_TOKEN_UNKNOWN_TOKEN,           /**< @brief Unknown/unrecognized token. */

} wo_lspv2_lexer_token;

/** @brief Opaque handle to the LSP lexer. */
typedef struct _wo_lspv2_lexer wo_lspv2_lexer;

/** @brief Information about a single lexer token. */
typedef struct _wo_lspv2_token_info
{
    wo_lspv2_lexer_token m_token;   /**< @brief Token type. */
    const void* m_token_serial;     /**< @brief Token serialized representation. */
    size_t m_token_length;          /**< @brief Length of the token text. */
    wo_lspv2_location m_location;   /**< @brief Source location of the token. */

} wo_lspv2_token_info;

/**
 * @brief Create a lexer for tokenizing Woolang source code.
 * @param src  The source code to tokenize.
 * @return A new lexer handle.
 */
WO_API wo_lspv2_lexer* wo_lspv2_lexer_create(const char* src);

/** @brief Destroy a lexer handle. */
WO_API void wo_lspv2_lexer_free(wo_lspv2_lexer* lexer);

/**
 * @brief Peek at the next token without consuming it.
 * @param lexer  The lexer handle.
 * @return Token info for the next token.
 */
WO_API wo_lspv2_token_info* wo_lspv2_lexer_peek(wo_lspv2_lexer* lexer);

/**
 * @brief Consume (advance past) the current token.
 * @param lexer  The lexer handle.
 */
WO_API void wo_lspv2_lexer_consume(wo_lspv2_lexer* lexer);

/** @brief Free token info. */
WO_API void wo_lspv2_token_info_free(wo_lspv2_token_info* info);

/**@}*/

#endif /* WO_NEED_LSP_API || WO_NEED_LSP_LEXER_API */


#if defined(WO_NEED_LSP_API)
/**
 * @brief LSP API provides metadata about Woolang source code for language tooling.
 *
 * All functions prefixed with wo_lspv2_ are used to retrieve structured information
 * about compiled source code: scopes, symbols, types, expressions, and macros.
 */

/** @brief Severity level for LSP diagnostic messages. */
typedef enum _wo_lsp_error_level
{
    WO_LSP_ERROR,          /**< @brief Compilation error. */
    WO_LSP_INFORMATION,    /**< @brief Informational message. */

} wo_lsp_error_level;

/** @name LSPv2 Common Types */
/**@{*/

/** @brief Opaque handle to compiled source metadata. */
typedef struct _wo_lspv2_source_meta wo_lspv2_source_meta;

/** @brief Opaque handle to a lexical scope. */
typedef struct _wo_lspv2_scope wo_lspv2_scope;

/** @brief Opaque iterator over sub-scopes. */
typedef struct _wo_lspv2_scope_iter wo_lspv2_scope_iter;

/** @brief Opaque handle to a symbol definition. */
typedef struct _wo_lspv2_symbol wo_lspv2_symbol;

/** @brief Opaque iterator over symbols. */
typedef struct _wo_lspv2_symbol_iter wo_lspv2_symbol_iter;

/** @brief Information about a lexical scope. */
typedef struct _wo_lspv2_scope_info
{
    /* OPTIONAL */ const char* m_name; /**< @brief Namespace name, or NULL for anonymous scopes. */
    bool m_has_location;               /**< @brief Whether m_location is valid. */
    wo_lspv2_location m_location;      /**< @brief Scope span in source. */
} wo_lspv2_scope_info;

/** @brief Kind of a symbol definition. */
typedef enum _wo_lspv2_symbol_kind
{
    WO_LSPV2_SYMBOL_VARIBALE,  /**< @brief Variable binding. */
    WO_LSPV2_SYMBOL_TYPE,      /**< @brief Type definition. */
    WO_LSPV2_SYMBOL_ALIAS,     /**< @brief Type alias. */

} wo_lspv2_symbol_kind;

/** @brief Opaque handle to a resolved type. */
typedef struct _wo_lspv2_type wo_lspv2_type;

/** @brief Opaque handle to a compile-time constant value. */
typedef struct _wo_lspv2_constant wo_lspv2_constant;

/** @brief Information about a compile-time constant. */
typedef struct _wo_lspv2_constant_info
{
    const char* m_expr; /**< @brief Constant expression as a string. */

} wo_lspv2_constant_info;

/** @brief Kind of a template parameter. */
typedef enum _wo_lspv2_template_param_kind
{
    WO_LSPV2_TEMPLATE_PARAM_TYPE,      /**< @brief Type template parameter. */
    WO_LSPV2_TEMPLATE_PARAM_CONSTANT,  /**< @brief Constant template parameter. */

} wo_lspv2_template_param_kind;

/** @brief A template parameter declaration. */
typedef struct _wo_lspv2_template_param
{
    wo_lspv2_template_param_kind m_kind; /**< @brief Parameter kind (type or constant). */
    const char* m_name;                  /**< @brief Parameter name. */

} wo_lspv2_template_param;

/** @brief A template argument binding. */
typedef struct _wo_lspv2_template_argument
{
    wo_lspv2_template_param_kind m_kind;                    /**< @brief Argument kind (type or constant). */
    /* OPTIONAL */ wo_lspv2_type* m_type;                   /**< @brief Resolved type, or NULL for constant args. */
    /* OPTIONAL */ wo_lspv2_constant* m_constant_may_null;  /**< @brief Constant value, or NULL for type args. */

} wo_lspv2_template_argument;

/** @brief Information about a symbol definition. */
typedef struct _wo_lspv2_symbol_info
{
    wo_lspv2_symbol_kind m_type;                      /**< @brief Symbol kind. */
    const char* m_name;                               /**< @brief Symbol name. */
    size_t m_template_params_count;                   /**< @brief Number of template parameters. */
    const _wo_lspv2_template_param* m_template_params; /**< @brief Template parameter array. */
    bool m_has_location;                              /**< @brief Whether m_location is valid. */
    wo_lspv2_location m_location;                     /**< @brief Symbol definition location. */
} wo_lspv2_symbol_info;

/** @brief Opaque iterator over compile errors from LSP compilation. */
typedef struct _wo_lspv2_error_iter wo_lspv2_error_iter;

/** @brief A single LSP diagnostic message. */
typedef struct _wo_lspv2_error_info
{
    wo_lsp_error_level m_level;  /**< @brief Severity level. */
    size_t m_depth;              /**< @brief Nesting depth within the compilation unit. */
    const char* m_describe;      /**< @brief Human-readable error description. */
    wo_lspv2_location m_location; /**< @brief Source location of the error. */

} wo_lspv2_error_info;

/** @brief Opaque iterator over expression collections. */
typedef struct _wo_lspv2_expr_collection_iter wo_lspv2_expr_collection_iter;

/** @brief Opaque handle to a collection of expressions (one per source file). */
typedef struct _wo_lspv2_expr_collection wo_lspv2_expr_collection;

/** @brief Information about an expression collection. */
typedef struct _wo_lspv2_expr_collection_info
{
    const char* m_file_name; /**< @brief Source file name for this collection. */
} wo_lspv2_expr_collection_info;

/** @brief Resolved type information. */
typedef struct _wo_lspv2_type_info
{
    const char* m_name;                         /**< @brief Type display name. */
    wo_lspv2_symbol* m_type_symbol;             /**< @brief The symbol that defines this type. */
    size_t m_template_arguments_count;          /**< @brief Number of template arguments. */
    const wo_lspv2_template_argument* m_template_arguments; /**< @brief Template argument bindings. */
} wo_lspv2_type_info;

/** @brief Opaque iterator over expressions. */
typedef struct _wo_lspv2_expr_iter wo_lspv2_expr_iter;

/** @brief Opaque handle to a typed expression node. */
typedef struct _wo_lspv2_expr wo_lspv2_expr;

/** @brief Information about a typed expression. */
typedef struct _wo_lspv2_expr_info
{
    wo_lspv2_type* m_type;                              /**< @brief Resolved type of the expression. */
    wo_lspv2_location m_location;                       /**< @brief Source location of the expression. */
    /* OPTIONAL */ wo_lspv2_symbol* m_symbol_may_null;  /**< @brief Referenced symbol, if any. */
    bool m_is_value_expr;                               /**< @brief true if value expression, false if type expression. */
    /* OPTIONAL */ wo_lspv2_constant* m_const_value_may_null; /**< @brief Compile-time constant value, or NULL. */
    size_t m_template_arguments_count;                         /**< @brief Number of template argument bindings. */
    /* OPTIONAL */ const wo_lspv2_template_argument* m_template_arguments; /**< @brief Template arguments, or NULL. */
} wo_lspv2_expr_info;

/** @brief Detailed struct type member information. */
typedef struct _wo_lspv2_type_struct_info
{
    size_t m_member_count;         /**< @brief Number of struct members. */
    const char** m_member_names;   /**< @brief Array of member names. */
    wo_lspv2_type** m_member_types; /**< @brief Array of member types. */

} wo_lspv2_type_struct_info;

/** @brief Opaque handle to a macro definition. */
typedef struct _wo_lspv2_macro wo_lspv2_macro;

/** @brief Opaque iterator over macro definitions. */
typedef struct _wo_lspv2_macro_iter wo_lspv2_macro_iter;

/** @brief Information about a macro definition. */
typedef struct _wo_lspv2_macro_info
{
    const char* m_name;            /**< @brief Macro name. */
    wo_lspv2_location m_location;  /**< @brief Macro definition location. */
} wo_lspv2_macro_info;

/** @} */ /* end LSPv2 Common Types */

/* ========== LSPv2 Functions ========== */

/**
 * @brief Query the LSPv2 API sub-version.
 * @return Sub-version number for feature detection.
 */
WO_API size_t wo_lspv2_sub_version(void);

/**
 * @brief Compile source code to a source_meta for LSP analysis.
 * @param virtual_src_path  Virtual path of the source file.
 * @param src               Source code string to compile.
 * @return A wo_lspv2_source_meta handle, or NULL on failure.
 */
WO_API wo_lspv2_source_meta* wo_lspv2_compile_to_meta(
    const char* virtual_src_path,
    const char* src);

/**
 * @brief Free a source_meta handle.
 * @param meta  The source metadata to free.
 */
WO_API void wo_lspv2_meta_free(wo_lspv2_source_meta* meta);

/**
 * @brief Create an iterator over compile errors in a source_meta.
 * @param meta  The source metadata.
 * @return An error iterator, or NULL if no errors exist.
 */
WO_API /* OPTIONAL */ wo_lspv2_error_iter* wo_lspv2_compile_err_iter(
    wo_lspv2_source_meta* meta);

/**
 * @brief Advance the error iterator and return the next error.
 * @param iter  The error iterator.
 * @return The next error info, or NULL when exhausted.
 */
WO_API /* OPTIONAL */ wo_lspv2_error_info* wo_lspv2_compile_err_next(
    wo_lspv2_error_iter* iter);

/**
 * @brief Free an error info object.
 * @param info  The error info to free.
 */
WO_API void wo_lspv2_err_info_free(wo_lspv2_error_info* info);

/**
 * @brief Convert a token serial+length pair to a thread-local string.
 * @param p    Pointer to token serial data.
 * @param len  Length of the token data.
 * @return A thread-local C string, valid until the next call.
 */
WO_API const char* /* thread-local */ wo_lspv2_token_info_enstring(
    const void* p, size_t len);

/** @name LSP Macro API */
/**@{*/

/**
 * @brief Create an iterator over macro definitions in source metadata.
 * @param meta  The source metadata.
 * @return A macro iterator, or NULL if grammar analysis failed.
 */
WO_API /* OPTIONAL */ wo_lspv2_macro_iter*
wo_lspv2_meta_macro_iter(wo_lspv2_source_meta* meta);

/**
 * @brief Advance the macro iterator and return the next macro.
 * @param iter  The macro iterator.
 * @return The next macro handle, or NULL when exhausted.
 */
WO_API /* OPTIONAL */ wo_lspv2_macro* wo_lspv2_macro_next(wo_lspv2_macro_iter* iter);

/**
 * @brief Get information about a macro definition.
 * @param macro  The macro handle.
 * @return Macro info (must be freed with wo_lspv2_macro_info_free).
 */
WO_API wo_lspv2_macro_info* wo_lspv2_macro_get_info(wo_lspv2_macro* macro);

/**
 * @brief Free macro info.
 * @param info  The macro info to free.
 */
WO_API void wo_lspv2_macro_info_free(wo_lspv2_macro_info* info);

/**@}*/

/** @name LSP Scope API */
/**@{*/

/**
 * @brief Get the global scope from source metadata.
 * @param meta  The source metadata.
 * @return The global scope, or NULL if grammar analysis failed.
 */
WO_API /* OPTIONAL */ wo_lspv2_scope*
wo_lspv2_meta_get_global_scope(wo_lspv2_source_meta* meta);

/**
 * @brief Create an iterator over sub-scopes within a scope.
 * @param scope  The parent scope.
 * @return A sub-scope iterator.
 */
WO_API wo_lspv2_scope_iter* wo_lspv2_scope_sub_scope_iter(wo_lspv2_scope* scope);

/**
 * @brief Advance the sub-scope iterator and return the next scope.
 * @param iter  The sub-scope iterator.
 * @return The next scope handle, or NULL when exhausted.
 */
WO_API /* OPTIONAL */ wo_lspv2_scope* wo_lspv2_scope_sub_scope_next(wo_lspv2_scope_iter* iter);

/**
 * @brief Get information about a scope.
 * @param scope  The scope handle.
 * @return Scope info (must be freed with wo_lspv2_scope_info_free).
 */
WO_API wo_lspv2_scope_info* wo_lspv2_scope_get_info(wo_lspv2_scope* scope);

/**
 * @brief Free scope info.
 * @param info  The scope info to free.
 */
WO_API void wo_lspv2_scope_info_free(wo_lspv2_scope_info* info);

/**@}*/

/** @name LSP Symbol API */
/**@{*/

/**
 * @brief Create an iterator over symbols in a scope.
 * @param scope  The scope handle.
 * @return A symbol iterator.
 */
WO_API wo_lspv2_symbol_iter* wo_lspv2_scope_symbol_iter(wo_lspv2_scope* scope);

/**
 * @brief Advance the symbol iterator and return the next symbol.
 * @param iter  The symbol iterator.
 * @return The next symbol handle, or NULL when exhausted.
 */
WO_API /* OPTIONAL */ wo_lspv2_symbol* wo_lspv2_scope_symbol_next(wo_lspv2_symbol_iter* iter);

/**
 * @brief Get information about a symbol.
 * @param symbol  The symbol handle.
 * @return Symbol info (must be freed with wo_lspv2_symbol_info_free).
 */
WO_API wo_lspv2_symbol_info* wo_lspv2_symbol_get_info(wo_lspv2_symbol* symbol);

/**
 * @brief Free symbol info.
 * @param info  The symbol info to free.
 */
WO_API void wo_lspv2_symbol_info_free(wo_lspv2_symbol_info* info);

/**@}*/

/** @name LSP Expression API */
/**@{*/

/**
 * @brief Create an iterator over expression collections in source metadata.
 * @param meta  The source metadata.
 * @return An expression collection iterator.
 */
WO_API wo_lspv2_expr_collection_iter* wo_lspv2_meta_expr_collection_iter(
    wo_lspv2_source_meta* meta);

/**
 * @brief Advance the expression collection iterator.
 * @param iter  The collection iterator.
 * @return The next expression collection, or NULL when exhausted.
 */
WO_API /* OPTIONAL */ wo_lspv2_expr_collection* wo_lspv2_expr_collection_next(
    wo_lspv2_expr_collection_iter* iter);

/**
 * @brief Free an expression collection.
 * @param collection  The expression collection to free.
 */
WO_API void wo_lspv2_expr_collection_free(wo_lspv2_expr_collection* collection);

/**
 * @brief Get info about an expression collection.
 * @param collection  The expression collection.
 * @return Collection info (must be freed).
 */
WO_API wo_lspv2_expr_collection_info* wo_lspv2_expr_collection_get_info(
    wo_lspv2_expr_collection* collection);

/**
 * @brief Free expression collection info.
 * @param collection  The expression collection info to free.
 */
WO_API void wo_lspv2_expr_collection_info_free(wo_lspv2_expr_collection_info* collection);

/**
 * @brief Find expressions within a given source range in a collection.
 * @param collection  The expression collection.
 * @param begin_row   Start row.
 * @param begin_col   Start column.
 * @param end_row     End row.
 * @param end_col     End column.
 * @return An expression iterator, or NULL if no matches found.
 */
WO_API /* OPTIONAL */ wo_lspv2_expr_iter* wo_lspv2_expr_collection_get_by_range(
    wo_lspv2_expr_collection* collection,
    size_t begin_row,
    size_t begin_col,
    size_t end_row,
    size_t end_col);

/**
 * @brief Advance the expression iterator and return the next expression.
 * @param iter  The expression iterator.
 * @return The next expression handle, or NULL when exhausted.
 */
WO_API /* OPTIONAL */ wo_lspv2_expr* wo_lspv2_expr_next(wo_lspv2_expr_iter* iter);

/**
 * @brief Get information about an expression.
 * @param expr  The expression handle.
 * @return Expression info (must be freed).
 */
WO_API wo_lspv2_expr_info* wo_lspv2_expr_get_info(wo_lspv2_expr* expr);

/** @brief Free expression info. */
WO_API void wo_lspv2_expr_info_free(wo_lspv2_expr_info*);

/**@}*/

/** @name LSP Type API */
/**@{*/

/**
 * @brief Get information about a resolved type.
 * @param type  The type handle.
 * @param meta  The source metadata (for resolving references).
 * @return Type info (must be freed).
 */
WO_API wo_lspv2_type_info* wo_lspv2_type_get_info(
    wo_lspv2_type* type, wo_lspv2_source_meta* meta);

/** @brief Free type info. */
WO_API void wo_lspv2_type_info_free(wo_lspv2_type_info* info);

/**
 * @brief Get struct member information for a struct type.
 * @param type  The type handle.
 * @param meta  The source metadata.
 * @return Struct info, or NULL if the type is not a struct.
 */
WO_API /* OPTIONAL */ wo_lspv2_type_struct_info* wo_lspv2_type_get_struct_info(
    wo_lspv2_type* type, wo_lspv2_source_meta* meta);

/** @brief Free struct type info. */
WO_API void wo_lspv2_type_struct_info_free(wo_lspv2_type_struct_info* info);

/**@}*/

/** @name LSP Constant API */
/**@{*/

/**
 * @brief Get information about a compile-time constant.
 * @param constant  The constant handle.
 * @param meta      The source metadata.
 * @return Constant info (must be freed).
 */
WO_API wo_lspv2_constant_info* wo_lspv2_constant_get_info(
    wo_lspv2_constant* constant, wo_lspv2_source_meta* meta);

/** @brief Free constant info. */
WO_API void wo_lspv2_constant_info_free(wo_lspv2_constant_info* info);

/**@}*/

/** @name LSP Semantic Tokens API */
/**@{*/

/**
 * @brief Semantic token type for syntax highlighting.
 */
typedef enum _wo_lspv2_semantic_token_type
{
    WO_LSPV2_SEMANTIC_NAMESPACE,      /**< @brief Namespace name. */
    WO_LSPV2_SEMANTIC_TYPE,           /**< @brief Type reference or definition. */
    WO_LSPV2_SEMANTIC_ENUM,           /**< @brief Enum definition. */
    WO_LSPV2_SEMANTIC_STRUCT,         /**< @brief Struct definition. */
    WO_LSPV2_SEMANTIC_TYPE_PARAMETER, /**< @brief Template type parameter. */
    WO_LSPV2_SEMANTIC_PARAMETER,      /**< @brief Function parameter. */
    WO_LSPV2_SEMANTIC_VARIABLE,       /**< @brief Variable or binding. */
    WO_LSPV2_SEMANTIC_PROPERTY,       /**< @brief Struct/union field member. */
    WO_LSPV2_SEMANTIC_FUNCTION,       /**< @brief Function or method. */
    WO_LSPV2_SEMANTIC_MACRO,          /**< @brief Macro name. */

} wo_lspv2_semantic_token_type;

/**
 * @brief Semantic token modifier flags.
 */
typedef enum _wo_lspv2_semantic_modifier
{
    WO_LSPV2_SEMANTIC_MOD_DECLARATION = 1 << 0, /**< @brief Token is a declaration. */
    WO_LSPV2_SEMANTIC_MOD_READONLY = 1 << 1, /**< @brief Token is read-only. */

} wo_lspv2_semantic_modifier;

/** @brief Opaque iterator over semantic tokens. */
typedef struct _wo_lspv2_semantic_token_iter wo_lspv2_semantic_token_iter;

/**
 * @brief Information about a single semantic token.
 */
typedef struct _wo_lspv2_semantic_token_info
{
    wo_lspv2_location m_location; /**< @brief Source location (includes file name). */
    uint32_t m_token_type;        /**< @brief One of wo_lspv2_semantic_token_type. */
    uint32_t m_modifiers;         /**< @brief Bitmask of wo_lspv2_semantic_modifier. */

} wo_lspv2_semantic_token_info;

/**
 * @brief Create an iterator over all semantic tokens in source metadata.
 * @param meta  The source metadata.
 * @return A semantic token iterator, or NULL if grammar analysis failed.
 */
WO_API /* OPTIONAL */ wo_lspv2_semantic_token_iter*
wo_lspv2_meta_get_semantic_token_iter(wo_lspv2_source_meta* meta);

/**
 * @brief Advance the iterator and return the next semantic token info.
 * @param iter  The semantic token iterator.
 * @return Next token info (free with wo_lspv2_semantic_token_info_free), or NULL when exhausted.
 */
WO_API /* OPTIONAL */ wo_lspv2_semantic_token_info*
wo_lspv2_semantic_token_next(wo_lspv2_semantic_token_iter* iter);

/**
 * @brief Free a semantic token info struct.
 * @param info  The token info to free.
 */
WO_API void wo_lspv2_semantic_token_info_free(wo_lspv2_semantic_token_info* info);

/**@}*/

#endif /* WO_NEED_LSP_API */

WO_FORCE_CAPI_END

/** @brief Undefine the public-facing WO_API to allow redefinition for internal use. */
#undef WO_API

/**
 * @brief Redefine WO_API for internal implementation use (always dllexport).
 *
 * This definition is only active after the public API declarations are closed,
 * and is used by the woolang implementation source files.
 */
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

#endif /* End of WO_MSVC_RC_INCLUDE */
/* NOTE: must contain an empty line following this line, .rc file will need it. */
