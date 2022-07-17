#pragma once
// Woolang Header
//
// Here will have wo c api;

#ifdef __cplusplus
#include <cstdint>
#include <cstddef>
#include <clocale>
#   define WO_FORCE_CAPI extern "C"{
#   define WO_FORCE_CAPI_END }
#else
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <locale.h>
#   define WO_FORCE_CAPI /* nothing */
#   define WO_FORCE_CAPI_END /* nothing */
#endif


#ifdef _WIN32
#   define WO_IMPORT __declspec(dllimport)
#   define WO_EXPORT __declspec(dllexport)
#else
#   define WO_IMPORT extern
#   define WO_EXPORT extern
#endif

#ifdef WO_IMPL
#   define WO_IMPORT_OR_EXPORT WO_EXPORT
#else
#   define WO_IMPORT_OR_EXPORT WO_IMPORT
#endif

#ifdef WO_STATIC_LIB
#   define WO_API
#else
#   define WO_API WO_IMPORT_OR_EXPORT
#endif

WO_FORCE_CAPI

typedef int64_t     wo_integer_t, wo_int_t;
typedef uint64_t    wo_handle_t;
typedef void* wo_ptr_t;
typedef const char* wo_string_t;
typedef double      wo_real_t;
typedef size_t      wo_result_t, wo_api, wo_size_t;
typedef bool        wo_bool_t;

#define WO_STRUCT_TAKE_PLACE(BYTECOUNT) uint8_t _take_palce_[BYTECOUNT]

typedef struct _wo_vm
{ /* reserved, and prevent from type casting. */
    WO_STRUCT_TAKE_PLACE(1);
}
*wo_vm;
typedef struct _wo_value
{ /* reserved, and prevent from type casting. */
    WO_STRUCT_TAKE_PLACE(16);
}
*wo_value;

typedef enum _wo_value_type
{
    WO_INVALID_TYPE = 0,

    WO_INTEGER_TYPE,
    WO_REAL_TYPE,
    WO_HANDLE_TYPE,

    WO_IS_REF,
    WO_CALLSTACK_TYPE,
    WO_NATIVE_CALLSTACK_TYPE,

    WO_NEED_GC_FLAG = 0xF0,

    WO_STRING_TYPE,
    WO_MAPPING_TYPE,
    WO_ARRAY_TYPE,
    WO_GCHANDLE_TYPE,
    WO_CLOSURE_TYPE,
    WO_STRUCT_TYPE
}
wo_type;

typedef wo_result_t(*wo_native_func)(wo_vm, wo_value, size_t);

typedef void(*wo_fail_handler)(wo_string_t src_file, uint32_t lineno, wo_string_t functionname, uint32_t rterrcode, wo_string_t reason);

WO_API wo_fail_handler wo_regist_fail_handler(wo_fail_handler new_handler);
WO_API void         wo_cause_fail(wo_string_t src_file, uint32_t lineno, wo_string_t functionname, uint32_t rterrcode, wo_string_t reason);

#define wo_fail(ERRID, REASON) ((void)wo_cause_fail(__FILE__, __LINE__, __func__,ERRID, REASON))

WO_API wo_string_t  wo_compile_date(void);
WO_API wo_string_t  wo_version(void);
WO_API wo_integer_t wo_version_int(void);

WO_API void         wo_init(int argc, char** argv);
#define wo_init(argc, argv) do{wo_init(argc, argv); setlocale(LC_CTYPE, wo_locale_name());}while(0)
WO_API void         wo_finish();

WO_API void         wo_gc_immediately();

WO_API wo_type      wo_valuetype(const wo_value value);

WO_API wo_integer_t wo_int(const wo_value value);
WO_API wo_real_t    wo_real(const wo_value value);
WO_API wo_handle_t  wo_handle(const wo_value value);
WO_API wo_ptr_t     wo_pointer(const wo_value value);
WO_API wo_string_t  wo_string(const wo_value value);
WO_API wo_bool_t    wo_bool(const wo_value value);
WO_API wo_value     wo_value_of_gchandle(wo_value value);
WO_API float        wo_float(const wo_value value);

WO_API wo_bool_t wo_is_ref(wo_value value);

WO_API void wo_set_int(wo_value value, wo_integer_t val);
WO_API void wo_set_real(wo_value value, wo_real_t val);
WO_API void wo_set_float(wo_value value, float val);
WO_API void wo_set_handle(wo_value value, wo_handle_t val);
WO_API void wo_set_pointer(wo_value value, wo_ptr_t val);
WO_API void wo_set_string(wo_value value, wo_string_t val);
WO_API void wo_set_bool(wo_value value, wo_bool_t val);
WO_API void wo_set_gchandle(wo_value value, wo_ptr_t resource_ptr, wo_value holding_val, void(*destruct_func)(wo_ptr_t));
WO_API void wo_set_val(wo_value value, wo_value val);
WO_API void wo_set_ref(wo_value value, wo_value val);
WO_API void wo_set_struct(wo_value value, uint16_t structsz);

WO_API wo_integer_t wo_cast_int(const wo_value value);
WO_API wo_real_t    wo_cast_real(const wo_value value);
WO_API wo_handle_t  wo_cast_handle(const wo_value value);
WO_API wo_ptr_t     wo_cast_pointer(const wo_value value);
WO_API wo_string_t  wo_cast_string(const wo_value value);
WO_API wo_string_t  wo_type_name(const wo_value value);
WO_API void         wo_cast_value_from_str(wo_value value, wo_string_t str, wo_type except_type);
WO_API float        wo_cast_float(const wo_value value);

WO_API wo_integer_t wo_argc(wo_vm vm);

WO_API wo_result_t  wo_ret_int(wo_vm vm, wo_integer_t result);
WO_API wo_result_t  wo_ret_real(wo_vm vm, wo_real_t result);
WO_API wo_result_t  wo_ret_float(wo_vm vm, float result);
WO_API wo_result_t  wo_ret_handle(wo_vm vm, wo_handle_t result);
WO_API wo_result_t  wo_ret_pointer(wo_vm vm, wo_ptr_t result);
WO_API wo_result_t  wo_ret_string(wo_vm vm, wo_string_t result);
WO_API wo_result_t  wo_ret_gchandle(wo_vm vm, wo_ptr_t resource_ptr, wo_value holding_val, void(*destruct_func)(wo_ptr_t));
#define wo_ret_void(vmm) (0)
WO_API wo_result_t  wo_ret_bool(wo_vm vm, wo_bool_t result);
WO_API wo_result_t  wo_ret_val(wo_vm vm, wo_value result);
WO_API wo_result_t  wo_ret_ref(wo_vm vm, wo_value result);
WO_API wo_result_t  wo_ret_dup(wo_vm vm, wo_value result);

WO_API wo_result_t  wo_ret_throw(wo_vm vm, wo_string_t reason);
WO_API wo_result_t  wo_ret_halt(wo_vm vm, wo_string_t reason);
WO_API wo_result_t  wo_ret_panic(wo_vm vm, wo_string_t reason);

WO_API wo_result_t  wo_ret_option_int(wo_vm vm, wo_integer_t result);
WO_API wo_result_t  wo_ret_option_real(wo_vm vm, wo_real_t result);
WO_API wo_result_t  wo_ret_option_float(wo_vm vm, float result);
WO_API wo_result_t  wo_ret_option_handle(wo_vm vm, wo_handle_t result);
WO_API wo_result_t  wo_ret_option_string(wo_vm vm, wo_string_t result);

WO_API wo_result_t  wo_ret_option_ptr(wo_vm vm, wo_ptr_t result);
WO_API wo_result_t  wo_ret_option_none(wo_vm vm);

WO_API wo_result_t  wo_ret_option_val(wo_vm vm, wo_value val);
WO_API wo_result_t  wo_ret_option_ref(wo_vm vm, wo_value val);
WO_API wo_result_t  wo_ret_option_gchandle(wo_vm vm, wo_ptr_t resource_ptr, wo_value holding_val, void(*destruct_func)(wo_ptr_t));

WO_API void         wo_coroutine_pauseall();
WO_API void         wo_coroutine_resumeall();
WO_API void         wo_coroutine_stopall();
WO_API void         wo_co_yield();
WO_API void         wo_co_sleep(double time);

typedef void* wo_waitter_t;
WO_API wo_waitter_t        wo_co_create_waitter();
WO_API void                wo_co_awake_waitter(wo_waitter_t waitter, void* val);
WO_API void*               wo_co_wait_for(wo_waitter_t waitter);

WO_API wo_integer_t wo_extern_symb(wo_vm vm, wo_string_t fullname);

WO_API void         wo_abort_all_vm_to_exit();

typedef enum _wo_inform_style
{
    WO_DEFAULT = 0,

    WO_NOTHING = 1,
    WO_NEED_COLOR = 2,
} wo_inform_style;

WO_API wo_string_t  wo_locale_name();
WO_API wo_string_t  wo_exe_path();

WO_API wo_bool_t    wo_virtual_source(wo_string_t filepath, wo_string_t data, wo_bool_t enable_modify);
WO_API wo_vm        wo_create_vm();
WO_API wo_vm        wo_sub_vm(wo_vm vm, size_t stacksz);
WO_API wo_vm        wo_gc_vm(wo_vm vm);
WO_API wo_bool_t    wo_abort_vm(wo_vm vm);
WO_API void         wo_close_vm(wo_vm vm);
WO_API wo_bool_t    wo_load_source(wo_vm vm, wo_string_t virtual_src_path, wo_string_t src);
WO_API wo_bool_t    wo_load_file(wo_vm vm, wo_string_t virtual_src_path);
WO_API wo_bool_t    wo_load_source_with_stacksz(wo_vm vm, wo_string_t virtual_src_path, wo_string_t src, size_t stacksz);
WO_API wo_bool_t    wo_load_file_with_stacksz(wo_vm vm, wo_string_t virtual_src_path, size_t stacksz);

WO_API wo_value     wo_run(wo_vm vm);

WO_API wo_bool_t    wo_has_compile_error(wo_vm vm);
WO_API wo_bool_t    wo_has_compile_warning(wo_vm vm);
WO_API wo_string_t  wo_get_compile_error(wo_vm vm, wo_inform_style style);
WO_API wo_string_t  wo_get_compile_warning(wo_vm vm, wo_inform_style style);

WO_API wo_string_t  wo_get_runtime_error(wo_vm vm);

WO_API wo_value     wo_push_int(wo_vm vm, wo_int_t val);
WO_API wo_value     wo_push_real(wo_vm vm, wo_real_t val);
WO_API wo_value     wo_push_handle(wo_vm vm, wo_handle_t val);
WO_API wo_value     wo_push_pointer(wo_vm vm, wo_ptr_t val);
WO_API wo_value     wo_push_gchandle(wo_vm vm, wo_ptr_t resource_ptr, wo_value holding_val, void(*destruct_func)(wo_ptr_t));
WO_API wo_value     wo_push_string(wo_vm vm, wo_string_t val);
WO_API wo_value     wo_push_empty(wo_vm vm);
WO_API wo_value     wo_push_val(wo_vm vm, wo_value val);
WO_API wo_value     wo_push_ref(wo_vm vm, wo_value val);
WO_API wo_value     wo_push_valref(wo_vm vm, wo_value val);

WO_API wo_value     wo_top_stack(wo_vm vm);
WO_API void         wo_pop_stack(wo_vm vm);
WO_API wo_value     wo_invoke_rsfunc(wo_vm vm, wo_int_t rsfunc, wo_int_t argc);
WO_API wo_value     wo_invoke_exfunc(wo_vm vm, wo_handle_t exfunc, wo_int_t argc);
WO_API wo_value     wo_invoke_value(wo_vm vm, wo_value vmfunc, wo_int_t argc);

WO_API wo_value     wo_dispatch_rsfunc(wo_vm vm, wo_int_t rsfunc, wo_int_t argc);
WO_API wo_value     wo_dispatch_value(wo_vm vm, wo_value vmfunc, wo_int_t argc);

#define WO_HAPPEND_ERR ((wo_value)NULL)
#define WO_CONTINUE    ((wo_value)(void*)-1)
WO_API wo_value     wo_dispatch(wo_vm vm);
WO_API void         wo_break_yield(wo_vm vm);

WO_API wo_int_t     wo_lengthof(wo_value value);

WO_API wo_value     wo_struct_get(wo_value value, uint16_t offset);

WO_API void         wo_arr_resize(wo_value arr, wo_int_t newsz, wo_value init_val);
WO_API wo_value     wo_arr_insert(wo_value arr, wo_int_t place, wo_value val);
WO_API wo_value     wo_arr_add(wo_value arr, wo_value elem);
WO_API wo_value     wo_arr_get(wo_value arr, wo_int_t index);
WO_API wo_int_t     wo_arr_find(wo_value arr, wo_value elem);
WO_API void         wo_arr_remove(wo_value arr, wo_int_t index);
WO_API void         wo_arr_clear(wo_value arr);
WO_API wo_bool_t    wo_arr_is_empty(wo_value arr);

WO_API wo_bool_t    wo_map_find(wo_value map, wo_value index);
WO_API wo_value     wo_map_get_by_default(wo_value map, wo_value index, wo_value default_value);
WO_API wo_value     wo_map_get_or_default(wo_value map, wo_value index, wo_value default_value);
WO_API wo_value     wo_map_get(wo_value map, wo_value index);
WO_API wo_value     wo_map_set(wo_value map, wo_value index, wo_value val);
WO_API wo_bool_t    wo_map_remove(wo_value map, wo_value index);
WO_API void         wo_map_clear(wo_value map);
WO_API wo_bool_t    wo_map_is_empty(wo_value arr);

WO_API wo_bool_t    wo_gchandle_close(wo_value gchandle);

// Here to define RSRuntime code accroding to the type.

// Here to define RSRuntime debug tools API
typedef struct _wo_debuggee_handle
{ /* reserved, and prevent from type casting. */
    WO_STRUCT_TAKE_PLACE(1);
}
*wo_debuggee;
typedef void(*wo_debuggee_handler_func)(wo_debuggee, wo_vm, void*);

WO_API void         wo_gc_stop();
WO_API void         wo_gc_pause();
WO_API void         wo_gc_resume();

WO_API void         wo_attach_default_debuggee(wo_vm vm);
WO_API wo_bool_t    wo_has_attached_debuggee(wo_vm vm);
WO_API void         wo_disattach_debuggee(wo_vm vm);
WO_API void         wo_disattach_and_free_debuggee(wo_vm vm);
WO_API void         wo_break_immediately(wo_vm vm);
WO_API void         wo_handle_ctrl_c(void(*handler)(int));

WO_API wo_string_t  wo_debug_trace_callstack(wo_vm vm, size_t layer);

#if defined(WO_IMPL)
#define WO_NEED_RTERROR_CODES 1
#define WO_NEED_ANSI_CONTROL 1
#endif

#if defined(WO_NEED_RTERROR_CODES) 

// Not allowed to using UNKNOWN as a error type.
#define WO_FAIL_TYPE_MASK 0xF000

// Minor error:
// Following error will not cause deadly problem, and in most cases, there 
// will be a relatively proper default solution. The main reason for reporting
// these errors at runtime is due to errors and usage or usage habits. In order
// to avoid more serious problems, please correct them as soon as possible. 
//
// * If you are re-write fail-handler-function, you can ignore this error.
// * (But dont forget display it.)
//
#define WO_FAIL_MINOR 0xA000

#define WO_FAIL_DEBUGGEE_FAIL 0xA001

// Medium error:
// These errors are caused by incorrect coding, some of which may have default
// solutions, but the default solutions may cause domino-like chain errors. 
//
// * If you are re-write fail-handler-function, you may need throw it(or fallback).
//
#define WO_FAIL_MEDIUM 0xB000

// Heavy error:
// Such errors will make it difficult for the program to continue running.
// Due to the lack of an appropriate default solution, ignoring such errors will
// usually lead to other errors and even the program may crash.
//
// * If you are re-write fail-handler-function, you may need throw it(or fallback).
// * HEAVY_FAIL WILL ABORT VIRTUALMACHINE BY DEFAULT
#define WO_FAIL_HEAVY 0xC000

// Deadly error:
// This type of error is caused by complex reasons, any default solutions are useless,
// if continue working, the program may crash
//
// * If you are re-write fail-handler-function, you may need abort program.
#define WO_FAIL_DEADLY 0xD000

#define WO_FAIL_NOT_SUPPORT 0xD001
#define WO_FAIL_TYPE_FAIL 0xD001
#define WO_FAIL_ACCESS_NIL 0xD001
#define WO_FAIL_INDEX_FAIL 0xD002
#define WO_FAIL_CALL_FAIL 0xD003

// dEADLY 

#endif

#if defined(WO_NEED_ANSI_CONTROL)

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

WO_FORCE_CAPI_END
#undef WO_API

#ifdef _WIN32
#   ifdef __cplusplus
#       define WO_API extern "C" WO_EXPORT
#   else
#       define WO_API WO_EXPORT
#   endif
#else
#   ifdef __cplusplus
#       define WO_API extern "C"
#   else
#       define WO_API WO_EXPORT
#   endif
#endif

