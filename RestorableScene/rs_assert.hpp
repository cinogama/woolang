#pragma once

#include "rs_macro.hpp"

#define RS_IMPL
#include "rs.h"

#include <iostream>
#include <cstdlib>

#define rs_static_assert_size(VAR, SIZE) \
static_assert(sizeof(VAR) == SIZE, "'" #VAR "' should be " #SIZE " byte.")

[[noreturn]]
static void _rs_assert(const char* file, uint32_t line, const char* function, const char* judgement, const char* reason = nullptr)
{
    std::cerr << ANSI_HIR "Assert failed: " ANSI_RST << judgement << std::endl;
    if (reason)
        std::cerr << "\t" ANSI_HIY << reason << ANSI_RST << std::endl;

    std::cerr << "Function : " << function << std::endl;
    std::cerr << "File : " << file << std::endl;
    std::cerr << "Line : " << line << std::endl;
    abort();
}

static void _rs_warning(const char* file, uint32_t line, const char* function, const char* judgement, const char* reason = nullptr)
{
    std::cerr << ANSI_HIY "Warning: " ANSI_RST << judgement << std::endl;
    if (reason)
        std::cerr << "\t" ANSI_HIY << reason << ANSI_RST << std::endl;

    std::cerr << "Function : " << function << std::endl;
    std::cerr << "File : " << file << std::endl;
    std::cerr << "Line : " << line << std::endl;
}

#define rs_test(...) rs_macro_overload(rs_test,__VA_ARGS__)
#define rs_test_1(JUDGEMENT) ((void)((!!(JUDGEMENT))||(_rs_assert(__FILE__, __LINE__, __func__, #JUDGEMENT),0)))
#define rs_test_2(JUDGEMENT, REASON) ((void)((!!(JUDGEMENT))||(_rs_assert(__FILE__, __LINE__, __func__, #JUDGEMENT, REASON),0)))

#define rs_test_warn(...) rs_macro_overload(rs_test_warn,__VA_ARGS__)
#define rs_test_warn_1(JUDGEMENT) ((void)((!!(JUDGEMENT))||(_rs_warning(__FILE__, __LINE__, __func__, #JUDGEMENT),0)))
#define rs_test_warn_2(JUDGEMENT, REASON) ((void)((!!(JUDGEMENT))||(_rs_warning(__FILE__, __LINE__, __func__, #JUDGEMENT, REASON),0)))

#ifdef NDEBUG

#define rs_assert(...) ((void)0)
#define rs_assert_warn(...) ((void)0)
#define rs_asure(...) ((void)(__VA_ARGS__))
#define rs_asure_warn(...) ((void)(__VA_ARGS__))

#else

#define rs_assert(...) rs_test(__VA_ARGS__)
#define rs_assert_warn(...) rs_test_warn(__VA_ARGS__)

#define rs_asure(...) rs_test(__VA_ARGS__)
#define rs_asure_warn(...) rs_test_warn(__VA_ARGS__)

#endif

#define rs_error(REASON) ((void)_rs_assert(__FILE__, __LINE__, __func__, "Runtime error:" , REASON))
#define rs_warning(REASON) ((void)_rs_warning(__FILE__, __LINE__, __func__, "Runtime warning." , REASON))