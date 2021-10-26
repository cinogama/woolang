#pragma once

#include <iostream>
#include <cstdlib>

#include "rs_macro.hpp"

#define rs_static_assert_size(VAR, SIZE) \
static_assert(sizeof(VAR) == SIZE, "'" #VAR "' should be " #SIZE " byte.")

[[noreturn]]
static void _rs_assert(const char* file, uint32_t line, const char* function, const char* judgement, const char* reason = nullptr)
{
    std::cerr << "Assert failed: " << judgement << std::endl;
    if (reason)
        std::cerr << "\t" << reason << std::endl;

    std::cerr << "Function : " << function << std::endl;
    std::cerr << "File : " << file << std::endl;
    std::cerr << "Line : " << line << std::endl;
    abort();
}

#define rs_test(...) rs_macro_overload(rs_test,__VA_ARGS__)
#define rs_test_1(JUDGEMENT) ((void)((!!(JUDGEMENT))||(_rs_assert(__FILE__, __LINE__, __func__, #JUDGEMENT),0)))
#define rs_test_2(JUDGEMENT, REASON) ((void)((!!(JUDGEMENT))||(_rs_assert(__FILE__, __LINE__, __func__, #JUDGEMENT, REASON),0)))

#ifdef NDEBUG

#define rs_assert(...) ((void)0)

#else

#define rs_assert(...) rs_test(__VA_ARGS__)

#endif

#define rs_error(REASON) ((void)_rs_assert(__FILE__, __LINE__, __func__, "Runtime error." , REASON))
