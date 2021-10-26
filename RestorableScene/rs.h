#pragma once
// RestorableScene Header
//
// Here will have rs c api;

#ifdef __cplusplus
#include <cstdint>
#   define RS_FORCE_CAPI extern "C"
#else
#include <stdint.h>
#   define RS_FORCE_CAPI /* nothing */
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

#define RS_API RS_FORCE_CAPI RS_IMPORT_OR_EXPORT

typedef int64_t     rs_integer_t;
typedef uint64_t    rs_handle_t;
typedef void*       rs_pointer_t;
typedef const char* rs_string_t;
typedef double      rs_real_t;

typedef struct _rs_vm
{ /* reserved, and prevent from type casting. */
}
*rs_vm;
typedef struct _rs_value
{ /* reserved, and prevent from type casting. */
    uint8_t _take_palce_[16];
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
    RS_MAPPING_TYPE,
}
rs_type;

typedef void(*rs_native_func)(rs_vm);

typedef void(*rs_fail_handler)(rs_string_t src_file, uint32_t lineno, rs_string_t functionname, rs_string_t reason);

RS_API rs_fail_handler rs_regist_fail_handler(rs_fail_handler new_handler);
RS_API void         rs_cause_fail(rs_string_t src_file, uint32_t lineno, rs_string_t functionname, rs_string_t reason);

#define rs_fail(REASON) ((void)rs_cause_fail(__FILE__, __LINE__, __func__, REASON))

RS_API rs_string_t  rs_compile_date (void);
RS_API rs_string_t  rs_version      (void);
RS_API rs_integer_t rs_version_int  (void);

RS_API rs_type      rs_valuetype    (rs_value value);

RS_API rs_integer_t rs_integer(rs_value value);
RS_API rs_real_t    rs_real(rs_value value);
RS_API rs_handle_t  rs_handle(rs_value value);
RS_API rs_string_t  rs_string(rs_value value);

RS_API rs_integer_t rs_cast_integer(rs_value value);
RS_API rs_real_t    rs_cast_real(rs_value value);
RS_API rs_handle_t  rs_cast_handle(rs_value value);
RS_API rs_string_t  rs_cast_string(rs_value value);

RS_API rs_value*    rs_args(rs_vm vm);
RS_API rs_integer_t rs_argc(rs_vm vm);

#ifdef __cplusplus
#include <exception>

namespace rs
{
    class rs_runtime_error :public std::exception
    {
        rs_string_t _reason;
    public:
        rs_runtime_error(rs_string_t __reason) noexcept
            :_reason(__reason)
        {
        }

        virtual char const* what() const override
        {
            return _reason;
        }
    };
}

#endif