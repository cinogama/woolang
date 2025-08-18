#pragma once
// Woolang Header
//
// Here will have woolang c api;
//
#define WO_VERSION WO_VERSION_WRAP(1, 14, 11, 8)

#ifndef WO_MSVC_RC_INCLUDE

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

typedef int64_t wo_integer_t, wo_int_t;
typedef uint64_t wo_handle_t;
typedef void* wo_ptr_t;
typedef char32_t wo_wchar_t;
typedef const char* wo_string_t;
typedef const wo_wchar_t* wo_wstring_t;
typedef double wo_real_t;
typedef size_t wo_size_t;

#define WO_STRUCT_TAKE_PLACE(BYTECOUNT) uint8_t _take_palce_[BYTECOUNT]

typedef struct _wo_vm
{
    WO_STRUCT_TAKE_PLACE(1);
}
* wo_vm;
typedef struct _wo_value
{ /* reserved, and prevent from type casting. */
    WO_DECLARE_ALIGNAS(8)
        WO_STRUCT_TAKE_PLACE(16);
}
wo_unref_value, * wo_value;

typedef enum _wo_value_type_t
{
    WO_INVALID_TYPE = 0,

    WO_NEED_GC_FLAG = 1 << 7,

    WO_INTEGER_TYPE = 1,
    WO_REAL_TYPE = 2,
    WO_HANDLE_TYPE = 3,
    WO_BOOL_TYPE = 4,

    WO_SCRIPT_FUNC_TYPE = 5,
    WO_NATIVE_FUNC_TYPE = 6,

    WO_CALLSTACK_TYPE = 7,
    WO_FAR_CALLSTACK_TYPE = 8,
    WO_NATIVE_CALLSTACK_TYPE = 9,
    WO_YIELD_CHECK_POINT_TYPE = 10,

    WO_STRING_TYPE = WO_NEED_GC_FLAG | 1,
    WO_MAPPING_TYPE = WO_NEED_GC_FLAG | 2,
    WO_ARRAY_TYPE = WO_NEED_GC_FLAG | 3,
    WO_GCHANDLE_TYPE = WO_NEED_GC_FLAG | 4,
    WO_CLOSURE_TYPE = WO_NEED_GC_FLAG | 5,
    WO_STRUCT_TYPE = WO_NEED_GC_FLAG | 6,
} wo_type_t;

typedef enum _wo_reg
{
    WO_REG_T0,
    WO_REG_T1,
    WO_REG_T2,
    WO_REG_T3,
    WO_REG_T4,
    WO_REG_T5,
    WO_REG_T6,
    WO_REG_T7,
    WO_REG_T8,
    WO_REG_T9,
    WO_REG_T10,
    WO_REG_T11,
    WO_REG_T12,
    WO_REG_T13,
    WO_REG_T14,
    WO_REG_T15,

    WO_REG_R0,
    WO_REG_R1,
    WO_REG_R2,
    WO_REG_R3,
    WO_REG_R4,
    WO_REG_R5,
    WO_REG_R6,
    WO_REG_R7,
    WO_REG_R8,
    WO_REG_R9,
    WO_REG_R10,
    WO_REG_R11,
    WO_REG_R12,
    WO_REG_R13,
    WO_REG_R14,
    WO_REG_R15,

    WO_REG_CR,
    WO_REG_TC,
    WO_REG_ER,
    WO_REG_NI,
    WO_REG_PM,
    WO_REG_TP,
} wo_reg;

#ifdef WO_STRICTLY_BOOL
typedef enum _wo_bool
{
    WO_FALSE = 0,
    WO_TRUE = 1,
} wo_bool_t;
#else
typedef int wo_bool_t;
#define WO_FALSE (0)
#define WO_TRUE (1)
#endif

#define WO_CBOOL(EXPR) ((EXPR) ? WO_TRUE : WO_FALSE)

typedef enum _wo_api
{
    // [Returned by both VM, JIT and EXTERN function]
    // Normal end.
    // Nothing todo.
    WO_API_NORMAL = 0,

    // [Returned by VM]
    // Program ended by abort flag.
    // Nothing todo.
    WO_API_SIM_ABORT = 1,

    // [Returned by VM]
    // Program yield break, can be dispatched later.
    // Nothing todo.
    WO_API_SIM_YIELD = 2,

    // [Returned by EXTERN function]
    // External functions change the state of the virtual machine, 
    // which needs to be synchronized immediately
    //
    // If JIT received this: ip, sp, bp should be sync to vm state, 
    //      and return WO_API_SYNC immediately.
    // If VM received this: nothing todo.
    WO_API_RESYNC = 3,

    // [Returned by JIT function]
    // When the JIT function encounters an unhandled interrupt, or 
    // the state has changed from WO_API_RESYNC, it is passed upward 
    // via WO_API_SYNC
    //
    // If JIT received this: return WO_API_SYNC immediately.
    // If VM received this: refetch ip from vm state, then continue
    //      execute.
    WO_API_SYNC = 4,

} wo_api,
wo_result_t;

typedef wo_result_t(*wo_native_func_t)(wo_vm, wo_value);

typedef void (*wo_fail_handler_t)(
    wo_vm vm,
    wo_string_t src_file,
    uint32_t lineno,
    wo_string_t functionname,
    uint32_t rterrcode,
    wo_string_t reason);

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

typedef void (*wo_dylib_entry_func_t)(wo_dylib_handle_t);
typedef void (*wo_dylib_exit_func_t)(void);

typedef struct _wo_gc_work_context_t* wo_gc_work_context_t;

typedef void (*wo_gchandle_close_func_t)(wo_ptr_t);
typedef void (*wo_gcstruct_mark_func_t)(wo_gc_work_context_t, wo_ptr_t);

typedef void (*wo_debuggee_callback_func_t)(wo_vm, void*);

typedef struct _wo_extern_lib_func_pair
{
    wo_string_t m_name;
    void* m_func_addr;
} wo_extern_lib_func_t;

#define WO_EXTERN_LIB_FUNC_END \
    wo_extern_lib_func_t { nullptr, nullptr }
#define wo_fail(ERRID, ...)\
    ((void)wo_cause_fail(__FILE__, __LINE__, __func__, ERRID, __VA_ARGS__))
#define wo_execute_fail(VM, ERRID, REASON)\
    ((void)wo_execute_fail_handler(VM, __FILE__, __LINE__, __func__, ERRID, REASON))

WO_API wo_fail_handler_t wo_register_fail_handler(wo_fail_handler_t new_handler);
WO_API void wo_cause_fail(
    wo_string_t src_file,
    uint32_t lineno,
    wo_string_t functionname,
    uint32_t rterrcode,
    wo_string_t reasonfmt, ...);
WO_API void wo_execute_fail_handler(
    wo_vm vm,
    wo_string_t src_file,
    uint32_t lineno,
    wo_string_t functionname,
    uint32_t rterrcode,
    wo_string_t reason);

WO_API wo_string_t wo_commit_sha(void);
WO_API wo_string_t wo_compile_date(void);
WO_API wo_string_t wo_version(void);
WO_API wo_integer_t wo_version_int(void);

WO_API void wo_init(int argc, char** argv);
#define wo_init(argc, argv)                    \
    do                                         \
    {                                          \
        wo_init(argc, argv);                   \
        setlocale(LC_CTYPE, wo_locale_name()); \
    } while (0)
WO_API void wo_finish(void (*do_after_shutdown)(void*), void* custom_data);

WO_API void wo_gc_pause(void);
WO_API void wo_gc_resume(void);
WO_API void wo_gc_wait_sync(void);
WO_API void wo_gc_immediately(wo_bool_t fullgc);

WO_API wo_dylib_handle_t wo_fake_lib(
    const char* libname,
    const wo_extern_lib_func_t* funcs,
    wo_dylib_handle_t dependence_dylib_may_null);
WO_API wo_dylib_handle_t wo_load_lib(
    const char* libname,
    const char* path,
    const char* script_path,
    wo_bool_t panic_when_fail);
WO_API wo_dylib_handle_t wo_load_func(void* lib, const char* funcname);
WO_API void wo_unload_lib(wo_dylib_handle_t lib, wo_dylib_unload_method_t method_mask);

WO_API wo_type_t wo_valuetype(const wo_value value);
WO_API wo_bool_t wo_equal_byte(wo_value a, wo_value b);

// Woolang 1.13 NOTE: 
//      According to the Woolang calling convention, this method is only 
//      applicable for use within external interface functions declared 
//      as va-arg functions; And please make sure to call this function 
//      at the beginning of the interface to avoid counting contamination.
WO_API wo_integer_t wo_argc(wo_vm vm);

WO_API wo_wchar_t wo_char(wo_value value);
WO_API wo_integer_t wo_int(wo_value value);
WO_API wo_real_t wo_real(wo_value value);
WO_API wo_handle_t wo_handle(wo_value value);
WO_API wo_ptr_t wo_pointer(wo_value value);
WO_API wo_string_t wo_string(wo_value value);
WO_API wo_bool_t wo_bool(wo_value value);
WO_API float wo_float(wo_value value);
WO_API const void* wo_buffer(wo_value value, wo_size_t* bytelen);
#define wo_raw_string(value, bytelen) ((wo_string_t)wo_buffer(value, bytelen))

WO_API void wo_set_nil(wo_value value);
WO_API void wo_set_char(wo_value value, wo_wchar_t val);
WO_API void wo_set_int(wo_value value, wo_integer_t val);
WO_API void wo_set_real(wo_value value, wo_real_t val);
WO_API void wo_set_float(wo_value value, float val);
WO_API void wo_set_handle(wo_value value, wo_handle_t val);
WO_API void wo_set_pointer(wo_value value, wo_ptr_t val);
WO_API void wo_set_bool(wo_value value, wo_bool_t val);
WO_API void wo_set_val(wo_value value, wo_value val);

WO_API void wo_set_dup(wo_value value, wo_vm vm, wo_value val);

WO_API void wo_set_string(wo_value value, wo_vm vm, wo_string_t val);
WO_API void wo_set_string_fmt(wo_value value, wo_vm vm, wo_string_t fmt, ...);
WO_API void wo_set_buffer(wo_value value, wo_vm vm, const void* val, wo_size_t len);
#define wo_set_raw_string wo_set_buffer
WO_API void wo_set_gchandle(
    wo_value value,
    wo_vm vm,
    wo_ptr_t resource_ptr,
    wo_value holding_val_may_null,
    wo_gchandle_close_func_t destruct_func);
WO_API void wo_set_gcstruct(
    wo_value value,
    wo_vm vm,
    wo_ptr_t resource_ptr,
    wo_gcstruct_mark_func_t mark_func,
    wo_gchandle_close_func_t destruct_func);
WO_API void wo_set_struct(wo_value value, wo_vm vm, uint16_t structsz);
WO_API void wo_set_arr(wo_value value, wo_vm vm, wo_int_t count);
WO_API void wo_set_map(wo_value value, wo_vm vm, wo_size_t reserved);
WO_API void wo_set_union(wo_value value, wo_vm vm, wo_integer_t id, wo_value value_may_null);

WO_API wo_integer_t wo_cast_int(wo_value value);
WO_API wo_real_t wo_cast_real(wo_value value);
WO_API wo_handle_t wo_cast_handle(wo_value value);
WO_API wo_ptr_t wo_cast_pointer(wo_value value);
WO_API wo_bool_t wo_cast_bool(wo_value value);
WO_API float wo_cast_float(wo_value value);
WO_API wo_string_t wo_cast_string(wo_value value);

WO_API wo_bool_t wo_serialize(wo_value value, wo_string_t* out_str);
WO_API wo_bool_t wo_deserialize(wo_vm vm, wo_value value, wo_string_t str, wo_type_t except_type);

WO_API wo_string_t wo_type_name(wo_type_t value);

WO_API wo_result_t wo_ret_void(wo_vm vm);
WO_API wo_result_t wo_ret_char(wo_vm vm, wo_wchar_t result);
WO_API wo_result_t wo_ret_int(wo_vm vm, wo_integer_t result);
WO_API wo_result_t wo_ret_real(wo_vm vm, wo_real_t result);
WO_API wo_result_t wo_ret_float(wo_vm vm, float result);
WO_API wo_result_t wo_ret_handle(wo_vm vm, wo_handle_t result);
WO_API wo_result_t wo_ret_pointer(wo_vm vm, wo_ptr_t result);
WO_API wo_result_t wo_ret_bool(wo_vm vm, wo_bool_t result);
WO_API wo_result_t wo_ret_val(wo_vm vm, wo_value result);

WO_API wo_result_t wo_ret_string(wo_vm vm, wo_string_t result);
WO_API wo_result_t wo_ret_string_fmt(wo_vm vm, wo_string_t fmt, ...);
WO_API wo_result_t wo_ret_buffer(wo_vm vm, const void* result, wo_size_t len);
#define wo_ret_raw_string wo_ret_buffer
WO_API wo_result_t wo_ret_gchandle(
    wo_vm vm,
    wo_ptr_t resource_ptr,
    wo_value holding_val_may_null,
    wo_gchandle_close_func_t destruct_func);
WO_API wo_result_t wo_ret_gcstruct(
    wo_vm vm,
    wo_ptr_t resource_ptr,
    wo_gcstruct_mark_func_t mark_func,
    wo_gchandle_close_func_t destruct_func);
WO_API wo_result_t wo_ret_dup(wo_vm vm, wo_value result);

WO_API wo_result_t wo_ret_halt(wo_vm vm, wo_string_t reasonfmt, ...);
WO_API wo_result_t wo_ret_panic(wo_vm vm, wo_string_t reasonfmt, ...);
WO_API wo_result_t wo_ret_union(wo_vm vm, wo_integer_t id, wo_value value_may_null);

WO_API void wo_set_option_void(wo_value val, wo_vm vm);
WO_API void wo_set_option_char(wo_value val, wo_vm vm, wo_wchar_t result);
WO_API void wo_set_option_bool(wo_value val, wo_vm vm, wo_bool_t result);
WO_API void wo_set_option_int(wo_value val, wo_vm vm, wo_integer_t result);
WO_API void wo_set_option_real(wo_value val, wo_vm vm, wo_real_t result);
WO_API void wo_set_option_float(wo_value val, wo_vm vm, float result);
WO_API void wo_set_option_handle(wo_value val, wo_vm vm, wo_handle_t result);
WO_API void wo_set_option_string(wo_value val, wo_vm vm, wo_string_t result);
WO_API void wo_set_option_string_fmt(wo_value val, wo_vm vm, wo_string_t fmt, ...);
WO_API void wo_set_option_buffer(wo_value val, wo_vm vm, const void* result, wo_size_t len);
#define wo_set_option_raw_string wo_set_option_buffer
WO_API void wo_set_option_ptr_may_null(wo_value val, wo_vm vm, wo_ptr_t result);
WO_API void wo_set_option_pointer(wo_value val, wo_vm vm, wo_ptr_t result);
WO_API void wo_set_option_val(wo_value val, wo_vm vm, wo_value result);
WO_API void wo_set_option_gchandle(
    wo_value val,
    wo_vm vm,
    wo_ptr_t resource_ptr,
    wo_value holding_val_may_null,
    wo_gchandle_close_func_t destruct_func);
WO_API void wo_set_option_gcstruct(
    wo_value val,
    wo_vm vm,
    wo_ptr_t resource_ptr,
    wo_gcstruct_mark_func_t mark_func,
    wo_gchandle_close_func_t destruct_func);
WO_API void wo_set_option_none(wo_value val, wo_vm vm);

#define wo_set_ok_void wo_set_option_void
#define wo_set_ok_char wo_set_option_char
#define wo_set_ok_bool wo_set_option_bool
#define wo_set_ok_int wo_set_option_int
#define wo_set_ok_real wo_set_option_real
#define wo_set_ok_float wo_set_option_float
#define wo_set_ok_handle wo_set_option_handle
#define wo_set_ok_string wo_set_option_string
#define wo_set_ok_string_fmt wo_set_option_string_fmt
#define wo_set_ok_buffer wo_set_option_buffer
#define wo_set_ok_raw_string wo_set_option_buffer
#define wo_set_ok_pointer wo_set_option_pointer
#define wo_set_ok_val wo_set_option_val
#define wo_set_ok_gchandle wo_set_option_gchandle
#define wo_set_ok_gcstruct wo_set_option_gcstruct

WO_API void wo_set_err_void(wo_value val, wo_vm vm);
WO_API void wo_set_err_char(wo_value val, wo_vm vm, wo_wchar_t result);
WO_API void wo_set_err_bool(wo_value val, wo_vm vm, wo_bool_t result);
WO_API void wo_set_err_int(wo_value val, wo_vm vm, wo_integer_t result);
WO_API void wo_set_err_real(wo_value val, wo_vm vm, wo_real_t result);
WO_API void wo_set_err_float(wo_value val, wo_vm vm, float result);
WO_API void wo_set_err_handle(wo_value val, wo_vm vm, wo_handle_t result);
WO_API void wo_set_err_string(wo_value val, wo_vm vm, wo_string_t result);
WO_API void wo_set_err_string_fmt(wo_value val, wo_vm vm, wo_string_t fmt, ...);
WO_API void wo_set_err_buffer(wo_value val, wo_vm vm, const void* result, wo_size_t len);
#define wo_set_err_raw_string wo_set_option_buffer
WO_API void wo_set_err_pointer(wo_value val, wo_vm vm, wo_ptr_t result);
WO_API void wo_set_err_val(wo_value val, wo_vm vm, wo_value result);
WO_API void wo_set_err_gchandle(
    wo_value val,
    wo_vm vm,
    wo_ptr_t resource_ptr,
    wo_value holding_val_may_null,
    wo_gchandle_close_func_t destruct_func);
WO_API void wo_set_err_gcstruct(
    wo_value val,
    wo_vm vm,
    wo_ptr_t resource_ptr,
    wo_gcstruct_mark_func_t mark_func,
    wo_gchandle_close_func_t destruct_func);

WO_API wo_result_t wo_ret_option_void(wo_vm vm);
WO_API wo_result_t wo_ret_option_char(wo_vm vm, wo_wchar_t result);
WO_API wo_result_t wo_ret_option_bool(wo_vm vm, wo_bool_t result);
WO_API wo_result_t wo_ret_option_int(wo_vm vm, wo_integer_t result);
WO_API wo_result_t wo_ret_option_real(wo_vm vm, wo_real_t result);
WO_API wo_result_t wo_ret_option_float(wo_vm vm, float result);
WO_API wo_result_t wo_ret_option_handle(wo_vm vm, wo_handle_t result);
WO_API wo_result_t wo_ret_option_string(wo_vm vm, wo_string_t result);
WO_API wo_result_t wo_ret_option_string_fmt(wo_vm vm, wo_string_t fmt, ...);
WO_API wo_result_t wo_ret_option_buffer(wo_vm vm, const void* result, wo_size_t len);
#define wo_ret_option_raw_string wo_ret_option_buffer
WO_API wo_result_t wo_ret_option_ptr_may_null(wo_vm vm, wo_ptr_t result);
WO_API wo_result_t wo_ret_option_pointer(wo_vm vm, wo_ptr_t result);
WO_API wo_result_t wo_ret_option_val(wo_vm vm, wo_value result);
WO_API wo_result_t wo_ret_option_gchandle(
    wo_vm vm,
    wo_ptr_t resource_ptr,
    wo_value holding_val_may_null,
    wo_gchandle_close_func_t destruct_func);
WO_API wo_result_t wo_ret_option_gcstruct(
    wo_vm vm,
    wo_ptr_t resource_ptr,
    wo_gcstruct_mark_func_t mark_func,
    wo_gchandle_close_func_t destruct_func);
WO_API wo_result_t wo_ret_option_none(wo_vm vm);

#define wo_ret_ok_void wo_ret_option_void
#define wo_ret_ok_char wo_ret_option_char
#define wo_ret_ok_bool wo_ret_option_bool
#define wo_ret_ok_int wo_ret_option_int
#define wo_ret_ok_real wo_ret_option_real
#define wo_ret_ok_float wo_ret_option_float
#define wo_ret_ok_handle wo_ret_option_handle
#define wo_ret_ok_string wo_ret_option_string
#define wo_ret_ok_string_fmt wo_ret_option_string_fmt
#define wo_ret_ok_buffer wo_ret_option_buffer
#define wo_ret_ok_raw_string wo_ret_option_buffer
#define wo_ret_ok_pointer wo_ret_option_pointer
#define wo_ret_ok_val wo_ret_option_val
#define wo_ret_ok_gchandle wo_ret_option_gchandle
#define wo_ret_ok_gcstruct wo_ret_option_gcstruct

WO_API wo_result_t wo_ret_err_void(wo_vm vm);
WO_API wo_result_t wo_ret_err_char(wo_vm vm, wo_wchar_t result);
WO_API wo_result_t wo_ret_err_bool(wo_vm vm, wo_bool_t result);
WO_API wo_result_t wo_ret_err_int(wo_vm vm, wo_integer_t result);
WO_API wo_result_t wo_ret_err_real(wo_vm vm, wo_real_t result);
WO_API wo_result_t wo_ret_err_float(wo_vm vm, float result);
WO_API wo_result_t wo_ret_err_handle(wo_vm vm, wo_handle_t result);
WO_API wo_result_t wo_ret_err_string(wo_vm vm, wo_string_t result);
WO_API wo_result_t wo_ret_err_string_fmt(wo_vm vm, wo_string_t fmt, ...);
WO_API wo_result_t wo_ret_err_buffer(wo_vm vm, const void* result, wo_size_t len);
#define wo_ret_err_raw_string wo_ret_err_buffer
WO_API wo_result_t wo_ret_err_pointer(wo_vm vm, wo_ptr_t result);
WO_API wo_result_t wo_ret_err_val(wo_vm vm, wo_value result);
WO_API wo_result_t wo_ret_err_gchandle(
    wo_vm vm,
    wo_ptr_t resource_ptr,
    wo_value holding_val_may_null,
    wo_gchandle_close_func_t destruct_func);
WO_API wo_result_t wo_ret_err_gcstruct(
    wo_vm vm,
    wo_ptr_t resource_ptr,
    wo_gcstruct_mark_func_t mark_func,
    wo_gchandle_close_func_t destruct_func);

WO_API wo_result_t wo_ret_yield(wo_vm vm);

WO_API wo_bool_t wo_extern_symb(
    wo_value out_val,
    wo_vm vm,
    wo_string_t fullname);

WO_API void wo_abort_all_vm(void);

WO_API wo_string_t wo_locale_name(void);
WO_API wo_string_t wo_exe_path(void);
WO_API wo_string_t wo_work_path(void);
WO_API void wo_set_exe_path(wo_string_t path);
WO_API wo_bool_t wo_set_work_path(wo_string_t path);

// NOTE: Only used for 
WO_API wo_vm wo_set_this_thread_vm(wo_vm vm_may_null);

WO_API void wo_enable_jit(wo_bool_t option);
WO_API wo_bool_t wo_virtual_binary(
    wo_string_t filepath,
    const void* data,
    wo_size_t len,
    wo_bool_t enable_modify);
WO_API wo_bool_t wo_virtual_source(
    wo_string_t filepath,
    wo_string_t data,
    wo_bool_t enable_modify);

typedef struct _wo_virtual_file* wo_virtual_file_t;

WO_API wo_virtual_file_t wo_open_virtual_file(wo_string_t filepath);
WO_API wo_string_t wo_virtual_file_path(wo_virtual_file_t file);
WO_API const void* wo_virtual_file_data(wo_virtual_file_t file, size_t* len);
WO_API void wo_close_virtual_file(wo_virtual_file_t file);

typedef struct _wo_virtual_file_iter* wo_virtual_file_iter_t;
WO_API wo_virtual_file_iter_t wo_open_virtual_file_iter(void);
WO_API wo_string_t wo_next_virtual_file_iter(wo_virtual_file_iter_t iter);
WO_API void wo_close_virtual_file_iter(wo_virtual_file_iter_t iter);

WO_API wo_bool_t wo_remove_virtual_file(wo_string_t filepath);
WO_API wo_vm wo_create_vm(void);
WO_API wo_vm wo_sub_vm(wo_vm vm);
WO_API wo_bool_t wo_abort_vm(wo_vm vm);
WO_API void wo_close_vm(wo_vm vm);

// The function wo_borrow_vm/wo_release_vm just likes wo_sub_vm/wo_close_vm
// but the vm will managed by a pool to reduce the creation of vm.
WO_API wo_vm wo_borrow_vm(wo_vm vm);
WO_API void wo_release_vm(wo_vm vm);

WO_API wo_bool_t wo_load_source(wo_vm vm, wo_string_t virtual_src_path, wo_string_t src);
WO_API wo_bool_t wo_load_file(wo_vm vm, wo_string_t virtual_src_path);
WO_API wo_bool_t wo_load_binary(wo_vm vm, wo_string_t virtual_src_path, const void* buffer, wo_size_t length);

WO_API wo_bool_t wo_jit(wo_vm vm);

// NOTE: wo_dump_binary must invoke before wo_run.
WO_API void* wo_dump_binary(wo_vm vm, wo_bool_t saving_pdi, wo_size_t* out_length);
WO_API void wo_free_binary(void* buffer);

typedef void (*wo_execute_callback_ft)(wo_value, void*);
WO_API wo_bool_t wo_execute(wo_string_t src, wo_execute_callback_ft callback, void* data);

// wo_run is used for init a vm.
WO_API wo_value wo_run(wo_vm vm);

WO_API wo_bool_t wo_has_compile_error(wo_vm vm);
WO_API wo_string_t wo_get_compile_error(wo_vm vm, wo_inform_style_t style);

WO_API wo_string_t wo_get_runtime_error(wo_vm vm);

WO_API wo_value wo_register(wo_vm vm, wo_reg regid);

// Woolang 1.14: All stack push operations are removed, to reserve stack space,
//  use `wo_reserve_stack` instead.
// NOTE: vm's stack size might changed during `wo_reserve_stack`, because of this,
//  the parameters of external functions themselves may also need to be updated.
//  Please pass in the address of ARGS through the parameter inout_args.
// Best practices:
// 1) Invoke `wo_reserve_stack` at begin of external function.
//      WO_API wo_api example_function(wo_vm vm, wo_value args)
//      {
//          wo_value s = wo_reserve_stack(vm, 1, &args);
//          ...
// 2) Donot invoke `wo_reserve_stack` multiple times in one external function.
// 3) When reserving for another vm (not current vm), inout_args can use nullptr;
WO_API wo_value wo_reserve_stack(wo_vm vm, wo_size_t sz, wo_value* inout_args_maynull);
WO_API void wo_pop_stack(wo_vm vm, wo_size_t sz);

// Woolang 1.14: All invoke & dispatch function will not clean stack.
// NOTE: vm's stack size might changed during `wo_invoke_...`, because of this,
//  the parameters of external functions themselves may also need to be updated.
//  Please pass in the address of ARGS & RESERVED STACK through the parameter
//  inout_args and inout_s.
// Best practices:
// 1) Use data from updated args & reserved stack, avoid using old pointers.
// 2) Donot use inout_args_maynull & inout_s_maynull if invoke another vm (not current vm).
WO_API wo_value wo_invoke_value(
    wo_vm vm, wo_value vmfunc, wo_int_t argc, wo_value* inout_args_maynull, wo_value* inout_s_maynull);
WO_API void wo_dispatch_value(
    wo_vm vm, wo_value vmfunc, wo_int_t argc, wo_value* inout_args_maynull, wo_value* inout_s_maynull);

#define WO_ABORTED ((wo_value)NULL)
#define WO_CONTINUE ((wo_value)(void *)-1)

WO_API wo_value wo_dispatch(
    wo_vm vm, wo_value* inout_args_maynull, wo_value* inout_s_maynull);

// User gc struct API.

// Woolang 1.14.4, gcstruct is a gchandle with a mark callback. Users should perform
//  correct locking, marking and barrier operations through the following functions:
WO_API void wo_gcunit_lock(wo_value gc_reference_object);
WO_API void wo_gcunit_unlock(wo_value gc_reference_object);
WO_API void wo_gcunit_lock_shared_force(wo_value gc_reference_object);
WO_API void wo_gcunit_unlock_shared_force(wo_value gc_reference_object);

// ATTENTION: If woolang DONOT support WO_FORCE_GC_OBJ_THREAD_SAFETY, following
//  function will DO NOTHING, to lock shared forcely, you need use the force version.
//
// To ensure compatibility with WO_FORCE_GC_OBJ_THREAD_SAFETY mode, this function
//  still needs to be called when reading wo_value_storage of gcstruct
WO_API void wo_gcunit_lock_shared(wo_value gc_reference_object);
WO_API void wo_gcunit_unlock_shared(wo_value gc_reference_object);

// If overwriting a wo_value_storage in gcstruct, wo_gc_record_memory
//  must be executed before the operation.
WO_API void wo_gc_mark(wo_gc_work_context_t context, wo_value value_to_mark);
WO_API void wo_gc_mark_unit(wo_gc_work_context_t context, void* unitaddr);

WO_API void wo_gc_checkpoint(wo_vm vm);
WO_API void wo_gc_record_memory(wo_value val);

WO_API wo_bool_t wo_leave_gcguard(wo_vm vm);
WO_API wo_bool_t wo_enter_gcguard(wo_vm vm);

WO_API wo_size_t wo_str_char_len(wo_value value);
WO_API wo_size_t wo_str_byte_len(wo_value value);

WO_API wo_wchar_t wo_str_get_char(wo_string_t str, wo_size_t index);
WO_API wo_wchar_t wo_strn_get_char(wo_string_t str, wo_size_t size, wo_size_t index);

WO_API const wchar_t* wo_str_to_wstr(wo_string_t str);
WO_API const wchar_t* wo_strn_to_wstr(wo_string_t str, wo_size_t size);
WO_API wo_string_t wo_wstr_to_str(const wchar_t* str);
WO_API wo_string_t wo_wstrn_to_str(const wchar_t* str, wo_size_t size);

WO_API const char16_t* wo_str_to_u16str(wo_string_t str);
WO_API const char16_t* wo_strn_to_u16str(wo_string_t str, wo_size_t size);
WO_API wo_string_t wo_u16str_to_str(const char16_t* str);
WO_API wo_string_t wo_u16strn_to_str(const char16_t* str, wo_size_t size);

WO_API wo_wstring_t wo_str_to_u32str(wo_string_t str);
WO_API wo_wstring_t wo_strn_to_u32str(wo_string_t str, wo_size_t size);
WO_API wo_string_t wo_u32str_to_str(wo_wstring_t str);
WO_API wo_string_t wo_u32strn_to_str(wo_wstring_t str, wo_size_t size);

WO_API wo_size_t wo_struct_len(wo_value value);
WO_API wo_bool_t wo_struct_try_get(wo_value out_val, wo_value value, uint16_t offset);
WO_API wo_bool_t wo_struct_try_set(wo_value value, uint16_t offset, wo_value val);
WO_API void wo_struct_get(wo_value out_val, wo_value value, uint16_t offset);
WO_API void wo_struct_set(wo_value value, uint16_t offset, wo_value val);

WO_API wo_integer_t wo_union_get(wo_value out_val, wo_value unionval);
WO_API wo_bool_t wo_result_get(wo_value out_val, wo_value resultval);
#define wo_option_get wo_result_get

// Read operation
WO_API wo_size_t wo_arr_len(wo_value arr);
WO_API wo_bool_t wo_arr_try_get(wo_value out_val, wo_value arr, wo_size_t index);
WO_API void wo_arr_get(wo_value out_val, wo_value arr, wo_size_t index);
WO_API wo_bool_t wo_arr_front(wo_value out_val, wo_value arr);
WO_API wo_bool_t wo_arr_back(wo_value out_val, wo_value arr);
WO_API void wo_arr_front_val(wo_value out_val, wo_value arr);
WO_API void wo_arr_back_val(wo_value out_val, wo_value arr);
WO_API wo_bool_t wo_arr_find(wo_value arr, wo_value elem, wo_size_t* out_index);
WO_API wo_bool_t wo_arr_is_empty(wo_value arr);

WO_API wo_size_t wo_map_len(wo_value map);
WO_API wo_bool_t wo_map_find(wo_value map, wo_value index);
WO_API wo_bool_t wo_map_get_or_default(wo_value out_val, wo_value map, wo_value index, wo_value default_value);
WO_API wo_bool_t wo_map_try_get(wo_value out_val, wo_value map, wo_value index);
WO_API void wo_map_get(wo_value out_val, wo_value map, wo_value index);
WO_API wo_bool_t wo_map_is_empty(wo_value arr);
WO_API void wo_map_keys(wo_value out_val, wo_vm vm, wo_value map);
WO_API void wo_map_vals(wo_value out_val, wo_vm vm, wo_value map);

// Write operation
WO_API wo_bool_t wo_arr_pop_front(wo_value out_val, wo_value arr);
WO_API wo_bool_t wo_arr_pop_back(wo_value out_val, wo_value arr);
WO_API void wo_arr_pop_front_val(wo_value out_val, wo_value arr);
WO_API void wo_arr_pop_back_val(wo_value out_val, wo_value arr);

WO_API wo_bool_t wo_arr_try_set(wo_value arr, wo_size_t index, wo_value val);
WO_API void wo_arr_set(wo_value arr, wo_size_t index, wo_value val);
WO_API void wo_arr_resize(wo_value arr, wo_size_t newsz, wo_value init_val);
WO_API wo_bool_t wo_arr_insert(wo_value arr, wo_size_t place, wo_value val);
WO_API void wo_arr_add(wo_value arr, wo_value elem);
WO_API wo_bool_t wo_arr_remove(wo_value arr, wo_size_t index);
WO_API void wo_arr_clear(wo_value arr);

WO_API void wo_map_reserve(wo_value map, wo_size_t sz);
WO_API void wo_map_set(wo_value map, wo_value index, wo_value val);
WO_API wo_bool_t wo_map_get_or_set(
    wo_value out_val,
    wo_value map,
    wo_value index,
    wo_value default_value);
WO_API wo_result_t wo_ret_map_get_or_set_do(
    wo_vm vm,
    wo_value map,
    wo_value index,
    wo_value value_function,
    wo_value* inout_args_maynull,
    wo_value* inout_s_maynull);
WO_API wo_bool_t wo_map_remove(wo_value map, wo_value index);
WO_API void wo_map_clear(wo_value map);

WO_API wo_bool_t wo_gchandle_close(wo_value gchandle);

// Here to define RSRuntime debug tools API
typedef struct _wo_debuggee_handle* wo_debuggee;
typedef void (*wo_debuggee_handler_func)(wo_debuggee, wo_vm, void*);

WO_API void wo_attach_default_debuggee(void);
WO_API void wo_attach_user_debuggee(wo_debuggee_callback_func_t callback, void* userdata);
WO_API wo_bool_t wo_has_attached_debuggee(void);
WO_API void wo_detach_debuggee(void);
WO_API void wo_break_immediately(void);
WO_API void wo_break_specify_immediately(wo_vm vmm);
WO_API void wo_handle_ctrl_c(void (*handler)(int));
WO_API wo_string_t wo_debug_trace_callstack(wo_vm vm, wo_size_t layer);

WO_API wo_integer_t wo_crc64_u8(uint8_t byte, wo_integer_t crc);
WO_API wo_integer_t wo_crc64_str(wo_string_t text);
WO_API wo_integer_t wo_crc64_file(wo_string_t filepath);

// GC Reference Pin
typedef struct _wo_pin_value* wo_pin_value;

WO_API wo_pin_value wo_create_pin_value(void);
WO_API void wo_close_pin_value(wo_pin_value pin_value);
WO_API void wo_pin_value_set(wo_pin_value pin_value, wo_value val);
WO_API void wo_pin_value_set_dup(wo_pin_value pin_value, wo_value val);
WO_API void wo_pin_value_get(wo_value out_value, wo_pin_value pin_value);

// Weak Reference
typedef struct _wo_weak_ref* wo_weak_ref;
WO_API wo_weak_ref wo_create_weak_ref(wo_value val);
WO_API void wo_close_weak_ref(wo_weak_ref ref);
WO_API wo_bool_t wo_lock_weak_ref(wo_value out_val, wo_weak_ref ref);

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
    wo_size_t m_begin_location[2]; // An array stores row & col
    wo_size_t m_end_location[2];   // An array stores row & col
} wo_lspv2_location;

typedef struct _wo_lspv2_source_meta wo_lspv2_source_meta;
typedef struct _wo_lspv2_scope wo_lspv2_scope;
typedef struct _wo_lspv2_scope_iter wo_lspv2_scope_iter;
typedef struct _wo_lspv2_symbol wo_lspv2_symbol;
typedef struct _wo_lspv2_symbol_iter wo_lspv2_symbol_iter;

typedef struct _wo_lspv2_scope_info
{
    const char* m_name; // null if not namespace
    wo_bool_t m_has_location;
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
    wo_size_t m_template_params_count;
    const _wo_lspv2_template_param* m_template_params;
    wo_bool_t m_has_location;
    wo_lspv2_location m_location;
} wo_lspv2_symbol_info;

typedef struct _wo_lspv2_error_iter wo_lspv2_error_iter;
typedef struct _wo_lspv2_error_info
{
    wo_lsp_error_level m_level;
    wo_size_t m_depth;
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
    wo_bool_t m_is_value_expr;       // false if type.
    wo_lspv2_constant* m_const_value_may_null; // null if not const or is type.
    size_t m_template_arguments_count;
    const wo_lspv2_template_argument* m_template_arguments; // null if not template instance.
} wo_lspv2_expr_info;

typedef struct _wo_lspv2_type_struct_info
{
    wo_size_t m_member_count;
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

WO_API wo_size_t wo_lspv2_sub_version(void);

WO_API wo_lspv2_source_meta* wo_lspv2_compile_to_meta(
    wo_string_t virtual_src_path,
    wo_string_t src);
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
    wo_size_t begin_row,
    wo_size_t begin_col,
    wo_size_t end_row,
    wo_size_t end_col);
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
#define WO_FAIL_EXECUTE_FAIL 0xA002

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

#define WO_FAIL_USER_PANIC 0xD001
#define WO_FAIL_NOT_SUPPORT 0xD002
#define WO_FAIL_TYPE_FAIL 0xD003
#define WO_FAIL_ACCESS_NIL 0xD004
#define WO_FAIL_INDEX_FAIL 0xD005
#define WO_FAIL_CALL_FAIL 0xD006
#define WO_FAIL_BAD_LIB 0xD007
#define WO_FAIL_UNEXPECTED 0xD008
#define WO_FAIL_STACKOVERFLOW 0xD009
// dEADLY

#endif

#if defined(WO_NEED_ANSI_CONTROL)

// You can use this macro to specify ANSI_XXX as a wide character string.
// NOTE: After use, you MUST re-define as nothing.
#define ANSI_ESC "\033["
#define ANSI_END "m"

#define ANSI_RST ANSI_ESC "0m"
#define ANSI_HIL ANSI_ESC "1m"
#define ANSI_FAINT ANSI_ESC "2m"
#define ANSI_ITALIC ANSI_ESC "3m"

#define ANSI_UNDERLNE ANSI_ESC "4m"
#define ANSI_NUNDERLNE ANSI_ESC "24m"

#define ANSI_SLOW_BLINK ANSI_ESC "5m"
#define ANSI_FAST_BLINK ANSI_ESC "6m"
#define ANSI_INV ANSI_ESC "7m"
#define ANSI_FADE ANSI_ESC "8m"

#define ANSI_BLK ANSI_ESC "30m"
#define ANSI_GRY ANSI_ESC "1;30m"
#define ANSI_RED ANSI_ESC "31m"
#define ANSI_HIR ANSI_ESC "1;31m"
#define ANSI_GRE ANSI_ESC "32m"
#define ANSI_HIG ANSI_ESC "1;32m"
#define ANSI_YEL ANSI_ESC "33m"
#define ANSI_HIY ANSI_ESC "1;33m"
#define ANSI_BLU ANSI_ESC "34m"
#define ANSI_HIB ANSI_ESC "1;34m"
#define ANSI_MAG ANSI_ESC "35m"
#define ANSI_HIM ANSI_ESC "1;35m"
#define ANSI_CLY ANSI_ESC "36m"
#define ANSI_HIC ANSI_ESC "1;36m"
#define ANSI_WHI ANSI_ESC "37m"
#define ANSI_HIW ANSI_ESC "1;37m"

#define ANSI_BBLK ANSI_ESC "40m"
#define ANSI_BGRY ANSI_ESC "1;40m"
#define ANSI_BRED ANSI_ESC "41m"
#define ANSI_BHIR ANSI_ESC "1;41m"
#define ANSI_BGRE ANSI_ESC "42m"
#define ANSI_BHIG ANSI_ESC "1;42m"
#define ANSI_BYEL ANSI_ESC "43m"
#define ANSI_BHIY ANSI_ESC "1;43m"
#define ANSI_BBLU ANSI_ESC "44m"
#define ANSI_BHIB ANSI_ESC "1;44m"
#define ANSI_BMAG ANSI_ESC "45m"
#define ANSI_BHIM ANSI_ESC "1;45m"
#define ANSI_BCLY ANSI_ESC "46m"
#define ANSI_BHIC ANSI_ESC "1;46m"
#define ANSI_BWHI ANSI_ESC "47m"
#define ANSI_BHIW ANSI_ESC "1;47m"

#endif

/*
                GC-friendly extern function development rules

    Please adhere to the following rules to ensure that the external functions
  you write behave safely!

  1. Use `fast` extern-function, it's safe.
  2. When writing `slow` extern-function, donot overwrite, pop or remove a gc-unit
    from any value unless following one of the following rules:

    2.1. Temporarily enter gc guard by calling `wo_enter_gcguard`, and if the function
        returns true, call `wo_leave_gcguard` after operation.
    2.2. Invoke `wo_gc_checkpoint` or `wo_gc_record_memory` before the operation.
    2.3. This gc-unit not be referenced elsewhere and discarded completely.

                                                            Cinogama project.
                                                                2024.3.15.
*/

#if defined(WO_NEED_OPCODE_API)

enum _wo_opcode
{
    /*
      CODE VAL     DR-OPNUM-FORMAT                      DESCRIPTION
    */
    WO_NOP = 0, // DR: Byte count
    // -- No OPNUM --                       Donothing, and skip next `DR`(0~3) byte codes.
    WO_MOV = 1, // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       Move value from `OPNUM2` to `OPNUM1`.
    WO_PSH = 2, // DRH: Opnum1 desc, DRL: Mode
    // OPNUM1: DRL = 1 ? RS/GLB : IMM_U16   If DRL == 1, Push `OPNUM1` to stack. or move
    //                                      the stack pointer by `OPNUM1` length, just like
    //                                      psh same count of garbage value
    WO_POP = 3, // DRH: Opnum1 desc, DRL: Mode
    // OPNUM1: DRL = 1 ? RS/GLB : IMM_U16   If DRL == 1, Pop value fron stack and store it
    //                                      into `OPNUM1` . or move the stack pointer by
    //                                      `OPNUM1` length, just like pop same times.

    WO_ADDI = 4, // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       Add `OPNUM2` to `OPNUM1` and store the result
    //                                      in `OPNUM1`. Operands should be integers
    WO_SUBI = 5, // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       Subtract `OPNUM1` from `OPNUM2` and store the
    //                                      result in `OPNUM1`. Operands should be integers.
    WO_MULI = 6, // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       Multiply `OPNUM1` by `OPNUM2` and store the
    //                                      result in `OPNUM1`. Operands should be integers.
    WO_DIVI = 7, // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       Divide `OPNUM1` by `OPNUM2` and store the result
    //                                      in `OPNUM1`. Operands should be integers.
    //                                      ATTENTION:
    //                                          There is no check for division by zero and
    //                                          division overflow in DIVI.
    WO_MODI = 8, // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       Get the remainder of `OPNUM1` divided by `OPNUM2`
    //                                      and store the result in `OPNUM1`. Operands should
    //                                      be integers.
    //                                      ATTENTION:
    //                                          There is no check for division by zero and
    //                                          division overflow in MODI.

    WO_ADDR = 9, // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       Add `OPNUM2` to `OPNUM1` and store the result
    //                                      in `OPNUM1`. Operands should be reals.
    WO_SUBR = 10,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       Subtract `OPNUM1` from `OPNUM2` and store the
    //                                      result in `OPNUM1`. Operands should be reals.
    WO_MULR = 11,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       Multiply `OPNUM1` by `OPNUM2` and store the
    //                                      result in `OPNUM1`. Operands should be reals.
    WO_DIVR = 12,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       Divide `OPNUM1` by `OPNUM2` and store the result
    //                                      in `OPNUM1`. Operands should be reals.
    WO_MODR = 13,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       Get the remainder of `OPNUM1` divided by `OPNUM2`
    //                                      and store the result in `OPNUM1`. Operands should
    //                                      be reals.

    WO_ADDH = 14,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       Add `OPNUM2` to `OPNUM1` and store the result
    //                                      in `OPNUM1`. Operands should be handles.
    WO_SUBH = 15,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       Subtract `OPNUM1` from `OPNUM2` and store the
    //                                      result in `OPNUM1`. Operands should be handles.

    WO_ADDS = 16,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       Concatenate `OPNUM2` to `OPNUM1` and store the
    //                                      result in `OPNUM1`. Operands should be strings.

    WO_IDARR = 17,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       Get the element of `OPNUM1` by index `OPNUM2`
    //                                      and store the result in register(cr).
    WO_SIDARR = 18,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       Set the element of `OPNUM1` by index `OPNUM2`
    // OPNUM3: RS                           with the value of `OPNUM3`.

    WO_IDDICT = 19,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       Get the value of `OPNUM2` by key `OPNUM2`
    //                                      and store the result in register(cr).
    WO_SIDDICT = 20,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       Set the value of `OPNUM2` by key `OPNUM2`
    // OPNUM3: RS                           with the value of `OPNUM3`. If the key is not
    //                                      exist, a panic will be triggered.
    WO_SIDMAP = 21,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       Set the value of `OPNUM2` by key `OPNUM2`
    // OPNUM3: RS                           with the value of `OPNUM3`.
    WO_IDSTRUCT = 22,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       Get the field of `OPNUM2` by index `OPNUM3`
    // OPNUM3: IMM_U16                      and store the result in `OPNUM1`.
    WO_SIDSTRUCT = 23,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       Set the field of `OPNUM1` by index `OPNUM3`
    // OPNUM3: IMM_U16                      with the value of `OPNUM2`.

    WO_IDSTR = 24,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       Get the character of `OPNUM1` by index `OPNUM2`
    //                                      and store the result in register(cr).

    WO_LTI = 25,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: IMM_U16      If `OPNUM1` less than `OPNUM2`, store true in
    //                                      register(cr), otherwise store false. Type of `OPNUM1`
    //                                      & `OPNUM2` should be integer.
    WO_GTI = 26,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: IMM_U16      If `OPNUM1` greater than `OPNUM2`, store true in
    //                                      register(cr), otherwise store false. Type of `OPNUM1`
    //                                      & `OPNUM2` should be integer.
    WO_ELTI = 27,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: IMM_U16      If `OPNUM1` less than or equal to `OPNUM2`, store
    //                                      true in register(cr), otherwise store false. Type of
    //                                      `OPNUM1` & `OPNUM2` should be integer.
    WO_EGTI = 28,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: IMM_U16      If `OPNUM1` greater than or equal to `OPNUM2`, store
    //                                      true in register(cr), otherwise store false. Type of
    //                                      `OPNUM1` & `OPNUM2` should be integer.

    WO_LAND = 29,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       If `OPNUM1` and `OPNUM2` are true, store true in
    //                                      register(cr), otherwise store false.
    WO_LOR = 30,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       If `OPNUM1` or `OPNUM2` are true, store true in
    //                                      register(cr), otherwise store false.

    WO_LTX = 31,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       If `OPNUM1` less than `OPNUM2`, store true in
    //                                      register(cr), otherwise store false. The type of `OPNUM1`
    //                                      and `OPNUM2` should be the same.
    WO_GTX = 32,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       If `OPNUM1` greater than `OPNUM2`, store true in
    //                                      register(cr), otherwise store false. The type of `OPNUM1`
    //                                      and `OPNUM2` should be the same.
    WO_ELTX = 33,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       If `OPNUM1` less than or equal to `OPNUM2`, store
    //                                      true in register(cr), otherwise store false. The type of
    //                                      `OPNUM1` and `OPNUM2` should be the same.

    WO_EGTX = 34,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       If `OPNUM1` greater than or equal to `OPNUM2`, store
    //                                      true in register(cr), otherwise store false. The type of
    //                                      `OPNUM1` and `OPNUM2` should be the same.

    WO_LTR = 35,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       If `OPNUM1` less than `OPNUM2`, store true in
    //                                      register(cr), otherwise store false. Type of `OPNUM1` &
    //                                      `OPNUM2` should be real.
    WO_GTR = 36,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       If `OPNUM1` greater than `OPNUM2`, store true in
    //                                      register(cr), otherwise store false. Type of `OPNUM1` &
    //                                      `OPNUM2` should be real.
    WO_ELTR = 37,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       If `OPNUM1` less than or equal to `OPNUM2`, store
    //                                      true in register(cr), otherwise store false. Type of
    //                                      `OPNUM1` & `OPNUM2` should be real.
    WO_EGTR = 38,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       If `OPNUM1` greater than or equal to `OPNUM2`, store
    //                                      true in register(cr), otherwise store false. Type of
    //                                      `OPNUM1` & `OPNUM2` should be real.

    WO_EQUR = 39,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       If `OPNUM1` equal to `OPNUM2`, store true in register(cr),
    //                                      otherwise store false. The type of `OPNUM1` and `OPNUM2`
    //                                      should be real.

    WO_NEQUR = 40,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       If `OPNUM1` not equal to `OPNUM2`, store true in register(cr),
    //                                      otherwise store false. The type of `OPNUM1` and `OPNUM2`
    //                                      should be real.
    WO_EQUS = 41,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       If `OPNUM1` equal to `OPNUM2`, store true in register(cr),
    //                                      otherwise store false. The type of `OPNUM1` and `OPNUM2`
    //                                      should be string.
    WO_NEQUS = 42,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       If `OPNUM1` not equal to `OPNUM2`, store true in register(cr),
    //                                      otherwise store false. The type of `OPNUM1` and `OPNUM2`
    //                                      should be string.
    WO_EQUB = 43,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       If `OPNUM1` equal to `OPNUM2`, store true in register(cr),
    //                                      otherwise store false. Only compare data field of value and
    //                                      ignore type.
    WO_NEQUB = 44,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       If `OPNUM1` not equal to `OPNUM2`, store true in register(cr),
    //                                      otherwise store false. Only compare data field of value and
    //                                      ignore type.

    WO_JNEQUB = 45,
    // DRH: Opnum1 desc, DRL: 0
    // OPNUM1: RS/GLB  OPNUM2: IMM_U16      If `OPNUM1` not equal to register(cr), jump to the instruction
    //                                      at the address `OPNUM2`. the compare method just like WO_EQUB.

    WO_CALL = 46,
    // DRH: Opnum1 desc, DRL: 0
    // OPNUM1: RS/GLB                       Push current bp & ip into stack, then:
    //                                      * if `OPNUM1` is integer, jump to target instruction.
    //                                      * if `OPNUM1` is handle, this is Woolang Native API Function.
    //                                          It will be invoked.
    //                                     * if `OPNUM1` is closure, the captured variable will be expand
    //                                          to stack, and the captured function will work following the
    //                                          above rules

    WO_CALLN = 47,
    // DRH: Leaving invoke flag, DRL: Woolang Native API Function flag.
    // OPNUM1: If DRL = 1 ? IMM_U64 : [IMM_U32 with 32bits padding]
    //                                      Absolute address call, the `OPNUM1` is the address of the function
    //                                      to be called. If DRL = 1, the function is a Woolang Native API
    //                                      Function. and in this case, if DRH = 1, the function will be
    //                                      invoked without gc-guard.
    //                                      If DRL = 0, DRH must be 0, and invoke just like integer case in
    //                                      WO_CALL.
    // _RESERVED_ = 48,
    // _RESERVED_ = 49,
    // _RESERVED_ = 50,
    WO_ENDPROC = 51,
    // DR: Mode flag
    // -- No OPNUM --                       * If DR = 0b00, debug command, an wo_error will be raise and
    //                                          the process will be aborted.
    //                                      * If DR = 0b10, vm will return from runing.
    //                                      * If DR = 0b01, Pop current bp & ip from stack, restore bp 
    //                                          then jump to the address of ip
    //                                      * If DR = 0b11, like 0b00, but pop `OPNUM1` values from stack.
    WO_MKCONTAIN = 52,
    // DRH: Opnum1 desc, DRL == 0 ? MakeArray : MakeMap
    // OPNUM1: RS/GLB  OPNUM2: IMM_U16     [Build an array]: Pop `OPNUM2` values from stack.
    //                                          The value in the stack should be:
    //                                              SP-> [Elem N-1, Elem N-2, ..., Elem 0] -> BP
    //                                      [Build a map]: Pop `OPNUM2` * 2 values from stack.
    //                                          The value in the stack should be:
    //                                              SP-> [Value N-1, Key N-1, ..., Value 0, Key 0] -> BP
    // _RESERVED_ = 53,
    WO_MKSTRUCT = 54,
    // DRH: Opnum1 desc, DRL: 0
    // OPNUM1: RS/GLB  OPNUM2: IMM_U16      Pop `OPNUM2` values from stack, Build a struct.
    //                                      The value in the stack should be:
    //                                          SP-> [Field N-1, Field N-2, ..., Field 0] -> BP
    WO_MKUNION = 55,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       Construct a struct of {+0: OPNUM3, +1: OPNUM2} and store the
    // OPNUM3: IMM_U16                      result in `OPNUM1`.
    WO_MKCLOS = 56,
    // DRH: Woolang Native API Function flag, DRL: 0
    // OPNUM1: IMM_U16  OPNUM2: DRH = 1 ? IMM_U64 : [IMM_U32 with 32bits padding]
    //                                      Pop `OPNUM1` values from stack, Build a closure for function
    //                                      `OPNUM2`.
    //                                      The value in the stack should be:
    //                                          SP-> [Captured N-1, Captured N-2, ..., Captured 0] -> BP
    //                                      Captured 0 will be [bp - 1] when this closure is invoked.

    WO_UNPACK = 57,
    // DRH: Opnum1 desc, DRL: 0
    // OPNUM1: RS/GLB  OPNUM2: IMM_U32      Expand array/struct into stack, at least expand abs(OPNUM2).
    //                                      * If reintp_cast<IMM_32>(OPNUM2) <= 0, expand count will be
    //                                          append into register(tc).
    WO_MOVCAST = 58,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       Move value from `OPNUM2` to `OPNUM1`, and cast the type
    // OPNUM3: IMM_U8                       of `OPNUM2` to the type `OPNUM3`.
    WO_TYPEAS = 59,
    // DRH: Opnum1 desc, DRL: Mode flag
    // OPNUM1: RS/GLB  OPNUM2: IMM_U8       Check if the type of `OPNUM1` is equal to `OPNUM2`, then:
    //                                      * If DRL = 1, store true in register(cr) if equal, otherwise
    //                                          store false.
    //                                      * If DRL = 0, a panic will be triggered if not equal.
    WO_SETIP = 60,
    // DRH: JCOND_FLAG, DRL: JCOND_FLAG ? (0: JMPF, 1: JMPT) : 0
    // OPNUM1: IMM_U32                      Jump to the instruction at the address `OPNUM1`.
    WO_LDS = 61,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       Load value from [bp + `OPNUM2`] and store it in `OPNUM1`.
    WO_STS = 62,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       Store value of `OPNUM1` to [bp + `OPNUM2`].
    WO_EXT = 63,
    // DR: Extern opcode type page
    // OPNUM1: IMM_U8 ...                   Let vm execute extern opcode `OPNUM1` in `DR` page.
};

enum _wo_opcode_ext0
{
    WO_PANIC = 0,
    // DRH: Panic type, DRL: 0
    // OPNUM1: RS/GLB                       Trigger a panic with message `OPNUM1`.
    WO_PACK = 1,
    // DRH: Opnum1 desc, DRL: 0
    // OPNUM1: RS/GLB  OPNUM2: IMM_U16      Collect arguments from [bp + 2 + OPNUM2 + OPNUM3] to
    // OPNUM3: IMM_U16                      [bp + 2 + tc + OPNUM3 - 1], and store them into an array,
    //                                      then store the result into `OPNUM1`.
    WO_CDIVILR = 2,
    // DRH: Opnum1 desc, DRL: Opnum2 desc
    // OPNUM1: RS/GLB  OPNUM2: RS/GLB       Trigger a panic when:
    //                                      * OPNUM2 == 0
    //                                      * OPNUM1 == INT64_MIN and OPNUM2 == -1
    WO_CDIVIL = 3,
    // DRH: Opnum1 desc
    // OPNUM1: RS/GLB                       Trigger a panic when:
    //                                      * OPNUM1 == INT64_MIN
    WO_CDIVIR = 4,
    // DRH: Opnum1 desc
    // OPNUM1: RS/GLB                       Trigger a panic when:
    //                                      * OPNUM1 == 0
    //                                      * OPNUM1 == -1
    WO_CDIVIRZ = 5,
    // DRH: Opnum1 desc
    // OPNUM1: RS/GLB                       Trigger a panic when:
    //                                      * OPNUM1 == 0
    WO_POPN = 6,
    // DRH: Opnum1 desc
    // DRL: 0
    // -- No OPNUM --                       Pop N values from stack, N is the value of Opnum1.
};

enum _wo_opcode_ext3
{
    WO_FUNCBEGIN = 0,
    // DRH: 0, DRL: 0
    // -- No OPNUM --                       Flag the begin of a function.
    //                                      Cannot execute, or it will cause a panic.
    WO_FUNCEND = 1,
    // DRH: 0, DRL: 0
    // -- No OPNUM --                       Flag the end of a function.
    //                                      Cannot execute, or it will cause a panic.
};

typedef struct _wo_ir_compiler* wo_ir_compiler;

WO_API wo_ir_compiler wo_create_ir_compiler(void);
WO_API void wo_close_ir_compiler(wo_ir_compiler ircompiler);

WO_API void wo_ir_opcode(wo_ir_compiler compiler, uint8_t opcode, uint8_t drh, uint8_t drl);
WO_API void wo_ir_bind_tag(wo_ir_compiler compiler, wo_string_t name);

WO_API void wo_ir_int(wo_ir_compiler compiler, wo_integer_t val);
WO_API void wo_ir_real(wo_ir_compiler compiler, wo_real_t val);
WO_API void wo_ir_handle(wo_ir_compiler compiler, wo_handle_t val);
WO_API void wo_ir_string(wo_ir_compiler compiler, wo_string_t val);
WO_API void wo_ir_bool(wo_ir_compiler compiler, wo_bool_t val);
WO_API void wo_ir_glb(wo_ir_compiler compiler, int32_t offset);
WO_API void wo_ir_reg(wo_ir_compiler compiler, uint8_t regid);
WO_API void wo_ir_bp(wo_ir_compiler compiler, int8_t offset);
WO_API void wo_ir_tag(wo_ir_compiler compiler, wo_string_t name);

WO_API void wo_ir_immtag(wo_ir_compiler compiler, wo_string_t name);
WO_API void wo_ir_immu8(wo_ir_compiler compiler, uint8_t val);
WO_API void wo_ir_immu16(wo_ir_compiler compiler, uint16_t val);
WO_API void wo_ir_immu32(wo_ir_compiler compiler, uint32_t val);
WO_API void wo_ir_immu64(wo_ir_compiler compiler, uint64_t val);

WO_API void wo_load_ir_compiler(wo_vm vm, wo_ir_compiler compiler);

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
