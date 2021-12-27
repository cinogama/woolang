#pragma once
#include <iostream>
#include <string>
#include <clocale>
#include <memory>
#include <cstring>

#ifdef _WIN32
#       include <Windows.h>
#       undef max      // fucking windows.
#       undef min      // fucking windows.
#else
#       include <unistd.h>
#endif

namespace rs
{
    // ATTENTION:
    // RS will work in UTF-8 mode as default 
    // (Before Windows 10 build 17134, setlocale will not work when using UTF-8).

    inline std::locale rs_global_locale = std::locale::classic();

    inline void rs_init_locale(const char* local_type)
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
        if (nullptr == std::setlocale(LC_CTYPE, local_type))
        {
            std::cerr << ANSI_HIR "RS: " ANSI_RST "Unable to initialize locale character set environment: " << std::endl;
            std::cerr << "\t" << ANSI_HIY << local_type << ANSI_RST <<" is not a valid locale type." << std::endl;

            std::exit(-1);
        }
        rs_global_locale = std::locale(local_type);

        printf(ANSI_RST);
    }

    inline std::string wstr_to_str(const std::wstring& wstr)
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

    inline std::string get_file_loc(std::string path)
    {
        for (auto& ch : path)
        {
            if (ch == '\\')
                ch = '/';
        }

        for (size_t index = path.size(); index > 0; index--)
            if (path[index - 1] == '/')
                // find last '/' get substr and return..
                return path.substr(0, index);

        return "";
    }

    inline const char* exe_path()
    {
        const size_t MAX_PATH_LEN = 8192;
        static char _exe_path[MAX_PATH_LEN] = {};

        if (!_exe_path[0])
        {
#ifdef _WIN32
            GetModuleFileNameA(NULL, _exe_path, MAX_PATH_LEN);
#else
            readlink("/proc/self/exe", _exe_path, MAX_PATH_LEN);
#endif
            auto _only_file_loc = get_file_loc(_exe_path);
            memcpy(_exe_path, _only_file_loc.c_str(), _only_file_loc.size() + 1);
        }

        return _exe_path;
    }
    inline const char* work_path()
    {
        const size_t MAX_PATH_LEN = 8192;
        thread_local char _work_path[MAX_PATH_LEN] = {};
#ifdef _WIN32
        _work_path[GetCurrentDirectoryA(MAX_PATH_LEN, _work_path)] = '/';
#else
        getcwd(_work_path, MAX_PATH_LEN);
        _work_path[strlen(_work_path)] = '/';
#endif
        auto _only_file_loc = get_file_loc(_work_path);
        memcpy(_work_path, _only_file_loc.c_str(), _only_file_loc.size() + 1);

        return _work_path;
    }

}