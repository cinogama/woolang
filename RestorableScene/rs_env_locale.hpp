#pragma once
#include<iostream>
#include<string>
#include<clocale>

#ifdef _WIN32
#       include <Windows.h>
#endif

namespace rs
{
    // ATTENTION:
    // RS will work in UTF-8 mode as default 
    // (Before Windows 10 build 17134, setlocale will not work when using UTF-8).

    inline void rs_init_locale()
    {
        
// SUPPORT ANSI_CONTROL
#if defined(RS_NEED_ANSI_CONTROL) && defined(_WIN32)
        auto this_console_handle = GetStdHandle(STD_OUTPUT_HANDLE);
        if (this_console_handle != INVALID_HANDLE_VALUE)
        {
            DWORD console_mode = 0;
            if (GetConsoleMode(this_console_handle, &console_mode))
            {
                SetConsoleMode(this_console_handle, console_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
            }
        }
#endif

#ifdef _WIN32
#endif

        std::setlocale(LC_ALL, "en_US.UTF-8");
        if (errno != 0)
        {
            std::cerr << ANSI_HIR "RS: " ANSI_RST "Unable to initialize locale character set environment." << std::endl;
        }
    }

    inline std::string wstr_to_str(const std::wstring & wstr)
    {
        size_t mstr_byte_length = wcstombs(nullptr, wstr.c_str(), 0) + 1;
        char* mstr_buffer = new char[mstr_byte_length];
        memset(mstr_buffer, 0, mstr_byte_length);
        wcstombs(mstr_buffer, wstr.c_str(), mstr_byte_length);
        std::string result = mstr_buffer;
        delete[]mstr_buffer;

        return result;
    }

    inline std::wstring str_to_wstr(const std::string& str)
    {
        size_t wstr_length = mbstowcs(nullptr, str.c_str(), 0) + 1;
        wchar_t* wstr_buffer = new wchar_t[wstr_length];
        wmemset(wstr_buffer, 0, wstr_length);
        mbstowcs(wstr_buffer, str.c_str(), wstr_length);
        std::wstring result = wstr_buffer;
        delete[]wstr_buffer;

        return result;
    }
}