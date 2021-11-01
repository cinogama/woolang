#pragma once
#include <exception>
#include <cstdint>

namespace rs
{
    class rsruntime_exception : public std::exception
    {
    public:
        uint32_t exception_code;
        rsruntime_exception(int _exception_code, const char* exception_info = "rs runtime error.")
            : std::exception(exception_info)
            , exception_code(_exception_code)
        {

        }
    };
}