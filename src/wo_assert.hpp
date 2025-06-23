#pragma once
#define WO_IMPL
#include "wo.h"

#include "wo_macro.hpp"
#include "wo_io.hpp"

#define wo_static_assert_size(VAR, SIZE) \
static_assert(sizeof(VAR) == SIZE, "'" #VAR "' should be " #SIZE " byte.")

[[noreturn]]
void _wo_assert(
    const char* file, 
    uint32_t line, 
    const char* function, 
    const char* judgement, 
    const char* reason = nullptr);

void _wo_warning(
    const char* file, 
    uint32_t line, 
    const char* function, 
    const char* judgement, 
    const char* reason = nullptr);

#define wo_test(...) wo_macro_overload(wo_test,__VA_ARGS__)
#define wo_test_1(JUDGEMENT) ((void)((!!(JUDGEMENT))||(_wo_assert(__FILE__, __LINE__, __func__, #JUDGEMENT),0)))
#define wo_test_2(JUDGEMENT, REASON) ((void)((!!(JUDGEMENT))||(_wo_assert(__FILE__, __LINE__, __func__, #JUDGEMENT, REASON),0)))

#define wo_test_warn(...) wo_macro_overload(wo_test_warn,__VA_ARGS__)
#define wo_test_warn_1(JUDGEMENT) ((void)((!!(JUDGEMENT))||(_wo_warning(__FILE__, __LINE__, __func__, #JUDGEMENT),0)))
#define wo_test_warn_2(JUDGEMENT, REASON) ((void)((!!(JUDGEMENT))||(_wo_warning(__FILE__, __LINE__, __func__, #JUDGEMENT, REASON),0)))

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

#define wo_error(REASON) ((void)_wo_assert(__FILE__, __LINE__, __func__, "Runtime error:" , REASON))
#define wo_warning(REASON) ((void)_wo_warning(__FILE__, __LINE__, __func__, "Runtime warning." , REASON))