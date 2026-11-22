#pragma once
#define WO_IMPL
#include "wo.h"

#include "wo_macro.hpp"

#define wo_static_assert_size(VAR, SIZE) \
static_assert(sizeof(VAR) == SIZE, "'" #VAR "' should be " #SIZE " byte.")

namespace wo
{
    [[noreturn]]
    void assert_failure(
        const char* file,
        uint32_t line,
        const char* function,
        const char* judgement,
        const char* reason = nullptr);

    void warning_message(
        const char* file,
        uint32_t line,
        const char* function,
        const char* judgement,
        const char* reason = nullptr);
}

#define wo_test(...) wo_macro_overload(wo_test,__VA_ARGS__)
#define wo_test_1(JUDGEMENT) ((void)((!!(JUDGEMENT))||(::wo::assert_failure(__FILE__, __LINE__, __func__, #JUDGEMENT),0)))
#define wo_test_2(JUDGEMENT, REASON) ((void)((!!(JUDGEMENT))||(::wo::assert_failure(__FILE__, __LINE__, __func__, #JUDGEMENT, REASON),0)))

#define wo_test_warn(...) wo_macro_overload(wo_test_warn,__VA_ARGS__)
#define wo_test_warn_1(JUDGEMENT) ((void)((!!(JUDGEMENT))||(::wo::warning_message(__FILE__, __LINE__, __func__, #JUDGEMENT),0)))
#define wo_test_warn_2(JUDGEMENT, REASON) ((void)((!!(JUDGEMENT))||(::wo::warning_message(__FILE__, __LINE__, __func__, #JUDGEMENT, REASON),0)))

#ifdef NDEBUG
#   define WO_ENABLE_RUNTIME_CHECK 0
#else
#   define WO_ENABLE_RUNTIME_CHECK 1
#endif

#if WO_ENABLE_RUNTIME_CHECK == 0

#define wo_assert(...) ((void)0)
#define wo_assert_warn(...) ((void)0)
#define wo_assure(...) ((void)(__VA_ARGS__))
#define wo_assure_warn(...) ((void)(__VA_ARGS__))

#else

#define wo_assert(...) wo_test(__VA_ARGS__)
#define wo_assert_warn(...) wo_test_warn(__VA_ARGS__)

#define wo_assure(...) wo_test(__VA_ARGS__)
#define wo_assure_warn(...) wo_test_warn(__VA_ARGS__)

#endif

#define wo_error(REASON) ((void)::wo::assert_failure(__FILE__, __LINE__, __func__, "Runtime error:" , REASON))
#define wo_warning(REASON) ((void)::wo::warning_message(__FILE__, __LINE__, __func__, "Runtime warning." , REASON))

#ifdef NDEBUG
#   ifdef _MSC_VER
#       define wo_unreachable(REASON) __assume(false)
#   else
#       define wo_unreachable(REASON) __builtin_unreachable()
#   endif
#else
#   define wo_unreachable wo_error
#endif
