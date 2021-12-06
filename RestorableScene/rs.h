#pragma once
// RestorableScene Header
//
// Here will have rs c api;

#ifdef __cplusplus
#include <cstdint>
#   define RS_FORCE_CAPI extern "C"{
#   define RS_FORCE_CAPI_END }
#else
#include <stdint.h>
#include <stdbool.h>
#   define RS_FORCE_CAPI /* nothing */
#   define RS_FORCE_CAPI_END /* nothing */
#endif

#ifdef _WIN32
#   ifdef RS_IMPL
#       define RS_IMPORT_OR_EXPORT __declspec(dllexport)
#   else
#       define RS_IMPORT_OR_EXPORT __declspec(dllimport)
#   endif
#else
#       define RS_IMPORT_OR_EXPORT extern
#endif

#define RS_API RS_IMPORT_OR_EXPORT

RS_FORCE_CAPI

typedef int64_t     rs_integer_t;
typedef uint64_t    rs_handle_t;
typedef void*       rs_pointer_t;
typedef const char* rs_string_t;
typedef double      rs_real_t;
typedef size_t      rs_result_t, rs_api;

#define RS_STRUCT_TAKE_PLACE(BYTECOUNT) uint8_t _take_palce_[BYTECOUNT]

typedef struct _rs_vm
{ /* reserved, and prevent from type casting. */
    RS_STRUCT_TAKE_PLACE(1);
}
*rs_vm;
typedef struct _rs_value
{ /* reserved, and prevent from type casting. */
    RS_STRUCT_TAKE_PLACE(16);
}
*rs_value;

typedef enum _rs_value_type
{ 
    RS_INVALID_TYPE = 0,

    RS_INTEGER_TYPE,
    RS_REAL_TYPE,
    RS_HANDLE_TYPE,

    RS_IS_REF,
    RS_CALLSTACK_TYPE,

    RS_NEED_GC_FLAG = 0xF0,

    RS_STRING_TYPE,
    RS_ARRAY_TYPE,
    RS_MAPPING_TYPE,
}
rs_type;

typedef rs_result_t(*rs_native_func)(rs_vm, rs_value);

typedef void(*rs_fail_handler)(rs_string_t src_file, uint32_t lineno, rs_string_t functionname, uint32_t rterrcode, rs_string_t reason);

RS_API rs_fail_handler rs_regist_fail_handler(rs_fail_handler new_handler);
RS_API void         rs_cause_fail(rs_string_t src_file, uint32_t lineno, rs_string_t functionname, uint32_t rterrcode, rs_string_t reason);

#define rs_fail(ERRID, REASON) ((void)rs_cause_fail(__FILE__, __LINE__, __func__,ERRID, REASON))

RS_API rs_string_t  rs_compile_date (void);
RS_API rs_string_t  rs_version      (void);
RS_API rs_integer_t rs_version_int  (void);

RS_API rs_type      rs_valuetype    (const rs_value value);

RS_API rs_integer_t rs_int(const rs_value value);
RS_API rs_real_t    rs_real(const rs_value value);
RS_API rs_handle_t  rs_handle(const rs_value value);
RS_API rs_string_t  rs_string(const rs_value value);

RS_API rs_integer_t rs_cast_int(const rs_value value);
RS_API rs_real_t    rs_cast_real(const rs_value value);
RS_API rs_handle_t  rs_cast_handle(const rs_value value);
RS_API rs_string_t  rs_cast_string(const rs_value value);
RS_API rs_string_t  rs_type_name(const rs_value value);
RS_API rs_value*    rs_args(rs_vm vm);
RS_API rs_integer_t rs_argc(rs_vm vm);

RS_API rs_result_t  rs_ret_int(rs_vm vm, rs_integer_t result);
RS_API rs_result_t  rs_ret_real(rs_vm vm, rs_real_t result);
RS_API rs_result_t  rs_ret_handle(rs_vm vm, rs_handle_t result);
RS_API rs_result_t  rs_ret_string(rs_vm vm, rs_string_t result);
RS_API rs_result_t  rs_ret_nil(rs_vm vm);

RS_API rs_integer_t rs_lengthof(rs_value value);

// Here to define RSRuntime code accroding to the type.

// Here to define RSRuntime debug tools API
typedef struct _rs_debuggee_handle
{ /* reserved, and prevent from type casting. */
    RS_STRUCT_TAKE_PLACE(1);
}
*rs_debuggee;
typedef void(*rs_debuggee_handler_func)(rs_debuggee, rs_vm, void*);



#if defined(RS_IMPL)
#define RS_NEED_RTERROR_CODES 1
#define RS_NEED_ANSI_CONTROL 1
#endif

#if defined(RS_NEED_RTERROR_CODES) 

// Not allowed to using UNKNOWN as a error type.

// Minor error:
// Following error will not cause deadly problem, and in most cases, there 
// will be a relatively proper default solution. The main reason for reporting
// these errors at runtime is due to errors and usage or usage habits. In order
// to avoid more serious problems, please correct them as soon as possible. 
//
// * If you are re-write fail-handler-function, you can ignore this error.
// * (But dont forget display it.)
//
//#define RS_ERR_MINOR 0x9000

// Medium error:
// These errors are caused by incorrect coding, some of which may have default
// solutions, but the default solutions may cause domino-like chain errors. 
//
// * If you are re-write fail-handler-function, you may need throw it(or fallback).
//
//#define RS_ERR_MEDIUM 0xA000
#define RS_ERR_TYPE_FAIL 0xA001

// Heavy error:
// Such errors will make it difficult for the program to continue running.
// Due to the lack of an appropriate default solution, ignoring such errors will
// usually lead to other errors and even the program may crash.
//
// * If you are re-write fail-handler-function, you may need throw it(or fallback).
//
//#define RS_ERR_HEAVY 0xB000
#define RS_ERR_ACCESS_NIL 0xB001
#define RS_ERR_INDEX_FAIL 0xB002
#define RS_ERR_CALL_FAIL 0xB003

// Deadly error:
// This type of error is caused by complex reasons, any default solutions are useless,
// if continue working, the program may crash / 
//
// * If you are re-write fail-handler-function, you may need abort program.\
//#define RS_ERR_DEADLY 0xC000

// dEADLY 

#endif

#if defined(RS_NEED_ANSI_CONTROL)

// You can use this macro to specify ANSI_XXX as a wide character string.
// NOTE: After use, you MUST re-define ANSI_WIDE_CHAR_SIGN as nothing.
#define ANSI_WIDE_CHAR_SIGN /* NOTHING */ 

#define __RS_ANSI_CONCAT(A,B) A##B
#define _RS_ANSI_CONCAT(A,B) __RS_ANSI_CONCAT(A,B)
#define RS_ANSI_CONCAT(A,B) _RS_ANSI_CONCAT(A,B)

#define ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN,"\033[")
#define ANSI_END RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN,"m")


#define ANSI_RST        ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "0m")
#define ANSI_HIL        ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "1m")
#define ANSI_FAINT      ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "2m")
#define ANSI_ITALIC     ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "3m")

#define ANSI_UNDERLNE   ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "4m")
#define ANSI_NUNDERLNE  ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "24m")

#define ANSI_SLOW_BLINK      RS_ANSI_CONCAT(ANSI_ESC ANSI_WIDE_CHAR_SIGN, "5m")
#define ANSI_FAST_BLINK      RS_ANSI_CONCAT(ANSI_ESC ANSI_WIDE_CHAR_SIGN, "6m")
#define ANSI_INV        RS_ANSI_CONCAT(ANSI_ESC ANSI_WIDE_CHAR_SIGN, "7m")
#define ANSI_FADE       RS_ANSI_CONCAT(ANSI_ESC ANSI_WIDE_CHAR_SIGN, "8m")

#define ANSI_BLK       ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "30m")
#define ANSI_GRY       ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "1;30m")
#define ANSI_RED       ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "31m")
#define ANSI_HIR       ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "1;31m")
#define ANSI_GRE       ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "32m")
#define ANSI_HIG       ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "1;32m")
#define ANSI_YEL       ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "33m")
#define ANSI_HIY       ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "1;33m")
#define ANSI_BLU       ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "34m")
#define ANSI_HIB       ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "1;34m")
#define ANSI_MAG       ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "35m")
#define ANSI_HIM       ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "1;35m")
#define ANSI_CLY       ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "36m")
#define ANSI_HIC       ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "1;36m")
#define ANSI_WHI       ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "37m")
#define ANSI_HIW       ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "1;37m")

#define ANSI_BBLK       ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "40m")
#define ANSI_BGRY       ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "1;40m")
#define ANSI_BRED       ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "41m")
#define ANSI_BHIR       ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "1;41m")
#define ANSI_BGRE       ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "42m")
#define ANSI_BHIG       ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "1;42m")
#define ANSI_BYEL       ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "43m")
#define ANSI_BHIY       ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "1;43m")
#define ANSI_BBLU       ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "44m")
#define ANSI_BHIB       ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "1;44m")
#define ANSI_BMAG       ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "45m")
#define ANSI_BHIM       ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "1;45m")
#define ANSI_BCLY       ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "46m")
#define ANSI_BHIC       ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "1;46m")
#define ANSI_BWHI       ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "47m")
#define ANSI_BHIW       ANSI_ESC RS_ANSI_CONCAT(ANSI_WIDE_CHAR_SIGN, "1;47m")


#endif

RS_FORCE_CAPI_END
#undef RS_API

#ifdef __cplusplus
#   define RS_API extern "C" RS_IMPORT_OR_EXPORT
#else
#   define RS_API RS_IMPORT_OR_EXPORT
#endif