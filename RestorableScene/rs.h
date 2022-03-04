#pragma once
// RestorableScene Header
//
// Here will have rs c api;

#ifdef __cplusplus
#include <cstdint>
#include <cstddef>
#include <clocale>
#   define RS_FORCE_CAPI extern "C"{
#   define RS_FORCE_CAPI_END }
#else
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <locale.h>
#   define RS_FORCE_CAPI /* nothing */
#   define RS_FORCE_CAPI_END /* nothing */
#endif

#ifdef _WIN32
#   define RS_IMPORT __declspec(dllimport)
#   define RS_EXPORT __declspec(dllexport)
#else
#   define RS_IMPORT extern
#   define RS_EXPORT extern
#endif

#ifdef RS_IMPL
#   define RS_IMPORT_OR_EXPORT RS_EXPORT
#else
#   define RS_IMPORT_OR_EXPORT RS_IMPORT
#endif

#define RS_API RS_IMPORT_OR_EXPORT

RS_FORCE_CAPI

typedef int64_t     rs_integer_t, rs_int_t;
typedef uint64_t    rs_handle_t;
typedef void* rs_ptr_t;
typedef const char* rs_string_t;
typedef double      rs_real_t;
typedef size_t      rs_result_t, rs_api, rs_size_t;
typedef bool        rs_bool_t;

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
    RS_NATIVE_CALLSTACK_TYPE,

    RS_NEED_GC_FLAG = 0xF0,

    RS_STRING_TYPE,
    RS_MAPPING_TYPE,
    RS_ARRAY_TYPE,
    RS_GCHANDLE_TYPE,
}
rs_type;

typedef rs_result_t(*rs_native_func)(rs_vm, rs_value, size_t);

typedef void(*rs_fail_handler)(rs_string_t src_file, uint32_t lineno, rs_string_t functionname, uint32_t rterrcode, rs_string_t reason);

RS_API rs_fail_handler rs_regist_fail_handler(rs_fail_handler new_handler);
RS_API void         rs_cause_fail(rs_string_t src_file, uint32_t lineno, rs_string_t functionname, uint32_t rterrcode, rs_string_t reason);

#define rs_fail(ERRID, REASON) ((void)rs_cause_fail(__FILE__, __LINE__, __func__,ERRID, REASON))

RS_API rs_string_t  rs_compile_date(void);
RS_API rs_string_t  rs_version(void);
RS_API rs_integer_t rs_version_int(void);

RS_API void         rs_init(int argc, char** argv);
#define rs_init(argc, argv) do{rs_init(argc, argv); setlocale(LC_CTYPE, rs_locale_name());}while(0)
RS_API void         rs_finish();

RS_API void         rs_gc_immediately();

RS_API rs_type      rs_valuetype(const rs_value value);

RS_API rs_integer_t rs_int(const rs_value value);
RS_API rs_real_t    rs_real(const rs_value value);
RS_API rs_handle_t  rs_handle(const rs_value value);
RS_API rs_ptr_t     rs_pointer(const rs_value value);
RS_API rs_string_t  rs_string(const rs_value value);

RS_API void rs_set_int(rs_value value, rs_integer_t val);
RS_API void rs_set_real(rs_value value, rs_real_t val);
RS_API void rs_set_handle(rs_value value, rs_handle_t val);
RS_API void rs_set_pointer(rs_value value, rs_ptr_t val);
RS_API void rs_set_string(rs_value value, rs_string_t val);
RS_API void rs_set_val(rs_value value, rs_value val);
RS_API void rs_set_ref(rs_value value, rs_value val);

RS_API rs_integer_t rs_cast_int(const rs_value value);
RS_API rs_real_t    rs_cast_real(const rs_value value);
RS_API rs_handle_t  rs_cast_handle(const rs_value value);
RS_API rs_ptr_t     rs_cast_pointer(const rs_value value);
RS_API rs_string_t  rs_cast_string(const rs_value value);
RS_API rs_string_t  rs_type_name(const rs_value value);
RS_API rs_integer_t rs_argc(rs_vm vm);

RS_API rs_result_t  rs_ret_int(rs_vm vm, rs_integer_t result);
RS_API rs_result_t  rs_ret_real(rs_vm vm, rs_real_t result);
RS_API rs_result_t  rs_ret_handle(rs_vm vm, rs_handle_t result);
RS_API rs_result_t  rs_ret_pointer(rs_vm vm, rs_ptr_t result);
RS_API rs_result_t  rs_ret_string(rs_vm vm, rs_string_t result);
RS_API rs_result_t  rs_ret_gchandle(rs_vm vm, rs_ptr_t resource_ptr, rs_value holding_val, void(*destruct_func)(rs_ptr_t));
RS_API rs_result_t  rs_ret_nil(rs_vm vm);
RS_API rs_result_t  rs_ret_bool(rs_vm vm, rs_bool_t result);
RS_API rs_result_t  rs_ret_val(rs_vm vm, rs_value result);
RS_API rs_result_t  rs_ret_ref(rs_vm vm, rs_value result);

RS_API void         rs_coroutine_pauseall();
RS_API void         rs_coroutine_resumeall();
RS_API void         rs_coroutine_stopall();
RS_API void         rs_co_yield();

RS_API rs_integer_t rs_extern_symb(rs_vm vm, rs_string_t fullname);

RS_API void         rs_abort_all_vm_to_exit();

enum _rs_inform_style
{
    RS_DEFAULT = 0,

    RS_NOTHING = 1,
    RS_NEED_COLOR = 2,

};

RS_API rs_string_t  rs_locale_name();

RS_API rs_bool_t    rs_virtual_source(rs_string_t filepath, rs_string_t data, rs_bool_t enable_modify);
RS_API rs_vm        rs_create_vm();
RS_API rs_vm        rs_sub_vm(rs_vm vm);
RS_API rs_bool_t    rs_abort_vm(rs_vm vm);
RS_API void         rs_close_vm(rs_vm vm);
RS_API rs_bool_t    rs_load_source(rs_vm vm, rs_string_t virtual_src_path, rs_string_t src);
RS_API rs_bool_t    rs_load_file(rs_vm vm, rs_string_t virtual_src_path);
RS_API rs_value     rs_run(rs_vm vm);
RS_API rs_bool_t    rs_has_compile_error(rs_vm vm);
RS_API rs_bool_t    rs_has_compile_warning(rs_vm vm);
RS_API rs_string_t  rs_get_compile_error(rs_vm vm, _rs_inform_style style);
RS_API rs_string_t  rs_get_compile_warning(rs_vm vm, _rs_inform_style style);

RS_API rs_value     rs_push_int(rs_vm vm, rs_int_t val);
RS_API rs_value     rs_push_real(rs_vm vm, rs_real_t val);
RS_API rs_value     rs_push_handle(rs_vm vm, rs_handle_t val);
RS_API rs_value     rs_push_pointer(rs_vm vm, rs_ptr_t val);
RS_API rs_value     rs_push_string(rs_vm vm, rs_string_t val);
RS_API rs_value     rs_push_nil(rs_vm vm);
RS_API rs_value     rs_push_val(rs_vm vm, rs_value val);
RS_API rs_value     rs_push_ref(rs_vm vm, rs_value val);
RS_API rs_value     rs_push_valref(rs_vm vm, rs_value val);

RS_API rs_value     rs_top_stack(rs_vm vm);
RS_API void         rs_pop_stack(rs_vm vm);
RS_API rs_value     rs_invoke_rsfunc(rs_vm vm, rs_int_t rsfunc, rs_int_t argc);
RS_API rs_value     rs_invoke_exfunc(rs_vm vm, rs_handle_t exfunc, rs_int_t argc);
RS_API rs_value     rs_invoke_value(rs_vm vm, rs_value vmfunc, rs_int_t argc);

RS_API rs_int_t     rs_lengthof(rs_value value);
RS_API void         rs_arr_resize(rs_value arr, rs_int_t newsz, rs_value init_val);
RS_API rs_value     rs_arr_add(rs_value arr, rs_value elem);
RS_API rs_value     rs_arr_get(rs_value arr, rs_int_t index);
RS_API rs_int_t     rs_arr_find(rs_value arr, rs_value elem);
RS_API void         rs_arr_remove(rs_value arr, rs_int_t index);
RS_API void         rs_arr_clear(rs_value arr);

RS_API rs_bool_t    rs_map_find(rs_value arr, rs_value index);
RS_API rs_value     rs_map_get(rs_value arr, rs_value index);
RS_API rs_value     rs_map_read(rs_value arr, rs_value index);
RS_API rs_value     rs_map_get_default(rs_value arr, rs_value index, rs_value default_value);
RS_API rs_bool_t    rs_map_erase(rs_value arr, rs_value index);

RS_API rs_bool_t    rs_gchandle_close(rs_value gchandle);

// Here to define RSRuntime code accroding to the type.

// Here to define RSRuntime debug tools API
typedef struct _rs_debuggee_handle
{ /* reserved, and prevent from type casting. */
    RS_STRUCT_TAKE_PLACE(1);
}
*rs_debuggee;
typedef void(*rs_debuggee_handler_func)(rs_debuggee, rs_vm, void*);

RS_API void         rs_gc_stop();
RS_API void         rs_gc_pause();
RS_API void         rs_gc_resume();

RS_API void         rs_attach_default_debuggee(rs_vm vm);
RS_API rs_bool_t    rs_has_attached_debuggee(rs_vm vm);
RS_API void         rs_disattach_debuggee(rs_vm vm);
RS_API void         rs_disattach_and_free_debuggee(rs_vm vm);
RS_API void         rs_break_immediately(rs_vm vm);
RS_API void         rs_handle_ctrl_c(void(*handler)(int));

#if defined(RS_IMPL)
#define RS_NEED_RTERROR_CODES 1
#define RS_NEED_ANSI_CONTROL 1
#endif

#if defined(RS_NEED_RTERROR_CODES) 

// Not allowed to using UNKNOWN as a error type.
#define RS_FAIL_TYPE_MASK 0xF000

// Minor error:
// Following error will not cause deadly problem, and in most cases, there 
// will be a relatively proper default solution. The main reason for reporting
// these errors at runtime is due to errors and usage or usage habits. In order
// to avoid more serious problems, please correct them as soon as possible. 
//
// * If you are re-write fail-handler-function, you can ignore this error.
// * (But dont forget display it.)
//
#define RS_FAIL_MINOR 0xA000

#define RS_FAIL_DEBUGGEE_FAIL 0xA001

// Medium error:
// These errors are caused by incorrect coding, some of which may have default
// solutions, but the default solutions may cause domino-like chain errors. 
//
// * If you are re-write fail-handler-function, you may need throw it(or fallback).
//
#define RS_FAIL_MEDIUM 0xB000

#define RS_FAIL_TYPE_FAIL 0xB001

// Heavy error:
// Such errors will make it difficult for the program to continue running.
// Due to the lack of an appropriate default solution, ignoring such errors will
// usually lead to other errors and even the program may crash.
//
// * If you are re-write fail-handler-function, you may need throw it(or fallback).
//
#define RS_FAIL_HEAVY 0xC000

#define RS_FAIL_ACCESS_NIL 0xC001
#define RS_FAIL_INDEX_FAIL 0xC002
#define RS_FAIL_CALL_FAIL 0xC003

// Deadly error:
// This type of error is caused by complex reasons, any default solutions are useless,
// if continue working, the program may crash
//
// * If you are re-write fail-handler-function, you may need abort program.
#define RS_FAIL_DEADLY 0xD000

#define RS_FAIL_NOT_SUPPORT 0xD001

// dEADLY 

#endif

#if defined(RS_NEED_ANSI_CONTROL)

// You can use this macro to specify ANSI_XXX as a wide character string.
// NOTE: After use, you MUST re-define as nothing.
#define ANSI_ESC "\033["
#define ANSI_END "m"


#define ANSI_RST        ANSI_ESC "0m"
#define ANSI_HIL        ANSI_ESC "1m"
#define ANSI_FAINT      ANSI_ESC "2m"
#define ANSI_ITALIC     ANSI_ESC "3m"

#define ANSI_UNDERLNE   ANSI_ESC "4m"
#define ANSI_NUNDERLNE  ANSI_ESC "24m"

#define ANSI_SLOW_BLINK      ANSI_ESC "5m"
#define ANSI_FAST_BLINK      ANSI_ESC "6m"
#define ANSI_INV        ANSI_ESC "7m"
#define ANSI_FADE       ANSI_ESC "8m"

#define ANSI_BLK       ANSI_ESC "30m"
#define ANSI_GRY       ANSI_ESC "1;30m"
#define ANSI_RED       ANSI_ESC "31m"
#define ANSI_HIR       ANSI_ESC "1;31m"
#define ANSI_GRE       ANSI_ESC "32m"
#define ANSI_HIG       ANSI_ESC "1;32m"
#define ANSI_YEL       ANSI_ESC "33m"
#define ANSI_HIY       ANSI_ESC "1;33m"
#define ANSI_BLU       ANSI_ESC "34m"
#define ANSI_HIB       ANSI_ESC "1;34m"
#define ANSI_MAG       ANSI_ESC "35m"
#define ANSI_HIM       ANSI_ESC "1;35m"
#define ANSI_CLY       ANSI_ESC "36m"
#define ANSI_HIC       ANSI_ESC "1;36m"
#define ANSI_WHI       ANSI_ESC "37m"
#define ANSI_HIW       ANSI_ESC "1;37m"

#define ANSI_BBLK       ANSI_ESC "40m"
#define ANSI_BGRY       ANSI_ESC "1;40m"
#define ANSI_BRED       ANSI_ESC "41m"
#define ANSI_BHIR       ANSI_ESC "1;41m"
#define ANSI_BGRE       ANSI_ESC "42m"
#define ANSI_BHIG       ANSI_ESC "1;42m"
#define ANSI_BYEL       ANSI_ESC "43m"
#define ANSI_BHIY       ANSI_ESC "1;43m"
#define ANSI_BBLU       ANSI_ESC "44m"
#define ANSI_BHIB       ANSI_ESC "1;44m"
#define ANSI_BMAG       ANSI_ESC "45m"
#define ANSI_BHIM       ANSI_ESC "1;45m"
#define ANSI_BCLY       ANSI_ESC "46m"
#define ANSI_BHIC       ANSI_ESC "1;46m"
#define ANSI_BWHI       ANSI_ESC "47m"
#define ANSI_BHIW       ANSI_ESC "1;47m"


#endif

RS_FORCE_CAPI_END
#undef RS_API

#ifdef _WIN32
#   ifdef __cplusplus
#       define RS_API extern "C" RS_EXPORT
#   else
#       define RS_API RS_EXPORT
#   endif
#else
#   ifdef __cplusplus
#       define RS_API extern "C"
#   else
#       define RS_API RS_EXPORT
#   endif
#endif

