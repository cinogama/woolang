#pragma once
#include <exception>
#include <cstdint>
#include <string>


namespace rs
{
    class rsruntime_exception : public std::exception
    {
        std::string reason;
    public:
        uint32_t exception_code;
        rsruntime_exception(int _exception_code, const char* exception_info = "rs runtime error.")
            : reason(exception_info)
            , exception_code(_exception_code)
        {

        }

        const char* what() const noexcept override
        {
            return reason.c_str();
        }
    };
}