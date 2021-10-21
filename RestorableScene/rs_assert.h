#pragma once

#include <iostream>
#include <cstdlib>

#include "rs_macro.h"

#define rs_static_assert_size(VAR, SIZE) \
static_assert(sizeof(VAR) == SIZE, "'" #VAR "' should be " #SIZE " byte.")

#ifdef NDEBUG

#define rs_assert(...) ((void)0)

#else

template<typename ... MSGTS>
void _rs_assert(const char* file, uint32_t line, const char* function, const char* judgement, const char* reason = nullptr)
{
    std::cerr << "Assert failed :" << judgement << std::endl;
    if (reason)
        std::cerr << "\t" << reason << std::endl;

    std::cerr << "Function : " << function << std::endl;
    std::cerr << "File : " << file << std::endl;
    std::cerr << "Line : " << line << std::endl;
    abort();
}

#define rs_assert(...) rs_macro_overload(rs_assert,__VA_ARGS__)
#define rs_assert_1(JUDGEMENT) ((void)((!!(JUDGEMENT))||(_rs_assert(__FILE__, __LINE__, __func__, #JUDGEMENT),0)))
#define rs_assert_2(JUDGEMENT, REASON) ((void)((!!(JUDGEMENT))||(_rs_assert(__FILE__, __LINE__, __func__, #JUDGEMENT, REASON),0)))

#endif