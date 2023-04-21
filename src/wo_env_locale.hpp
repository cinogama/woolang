#pragma once
#include "wo_io.hpp"

#include <iostream>
#include <string>
#include <clocale>
#include <memory>
#include <cstring>
#include <vector>

#ifdef _WIN32
#       include <Windows.h>
#       undef max      // fucking windows.
#       undef min      // fucking windows.
#else
#       include <unistd.h>
#endif

namespace wo
{
    // ATTENTION:
    // RS will work in UTF-8 mode as default 
    // (Before Windows 10 build 17134, setlocale will not work when using UTF-8).

    inline std::locale wo_global_locale = std::locale::classic();
    inline std::string wo_global_locale_name = "";

    inline std::vector<std::string> wo_args;

    inline void wo_init_args(int argc, char** argv)
    {
        wo_args.clear();
        for (int i = 0; i < argc; ++i)
            wo_args.push_back(argv[i]);
    }

    inline void wo_init_locale(const char* local_type)
    {

        // SUPPORT ANSI_CONTROL
#if defined(WO_NEED_ANSI_CONTROL) && defined(_WIN32)
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
            wo_stderr << ANSI_HIR "Woolang: " ANSI_RST "Unable to initialize locale character set environment: " << wo_endl;
            wo_stderr << "\t" << ANSI_HIY << local_type << ANSI_RST << " is not a valid locale type." << wo_endl;

            std::exit(-1);
        }
        wo_global_locale = std::locale(local_type);
        wo_global_locale_name = local_type;

        if (wo::config::ENABLE_OUTPUT_ANSI_COLOR_CTRL)
            printf(ANSI_RST);
    }

    inline wo_string_t wstr_to_str_ptr(const std::wstring& wstr)
    {
        size_t mstr_byte_length = wcstombs(nullptr, wstr.c_str(), 0);

        // Failed to parse, meet invalid string.
        if (mstr_byte_length == (size_t)-1)
            mstr_byte_length = 0;

        char* mstr_buffer = new char[mstr_byte_length + 1];
        wcstombs(mstr_buffer, wstr.c_str(), mstr_byte_length);
        mstr_buffer[mstr_byte_length] = 0;

        return mstr_buffer;
    }

    inline wo_wstring_t str_to_wstr_ptr(const std::string& str)
    {
        size_t wstr_length = mbstowcs(nullptr, str.c_str(), 0);

        // Failed to parse, meet invalid string.
        if (wstr_length == (size_t)-1)
            wstr_length = 0;

        wchar_t* wstr_buffer = new wchar_t[wstr_length + 1];
        mbstowcs(wstr_buffer, str.c_str(), wstr_length);
        wstr_buffer[wstr_length] = 0;

        return wstr_buffer;
    }

    inline std::string wstr_to_str(const std::wstring& wstr)
    {
        auto buf = wstr_to_str_ptr(wstr);
        std::string result = buf;
        delete[]buf;

        return result;
    }

    inline std::wstring str_to_wstr(const std::string& str)
    {
        auto buf = str_to_wstr_ptr(str);
        std::wstring result = buf;
        delete[]buf;

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
                return path.substr(0, index - 1);

        return "";
    }

    inline const char* exe_path()
    {
        const size_t MAX_PATH_LEN = 8192;
        static char _exe_path[MAX_PATH_LEN] = {};

        if (!_exe_path[0])
        {
#ifdef _WIN32
            wchar_t _w_exe_path[MAX_PATH_LEN / 2] = {};
            GetModuleFileNameW(NULL, _w_exe_path, MAX_PATH_LEN / 2);
            auto&& parsed_path = wstr_to_str(_w_exe_path);

            wo_assert(parsed_path.size() < MAX_PATH_LEN);
            strcpy(_exe_path, parsed_path.c_str());

#else
            [[maybe_unused]] auto reuslt = readlink("/proc/self/exe", _exe_path, MAX_PATH_LEN);
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
        auto write_length = GetCurrentDirectoryA(MAX_PATH_LEN - 2, _work_path);
#else
        [[maybe_unused]] auto reuslt = getcwd(_work_path, MAX_PATH_LEN - 2);
        auto write_length = strlen(_work_path);
#endif
        _work_path[write_length] = '/';
        _work_path[write_length + 1] = 0;

        auto _only_file_loc = get_file_loc(_work_path);
        memcpy(_work_path, _only_file_loc.c_str(), _only_file_loc.size() + 1);

        return _work_path;
    }
    inline bool set_work_path(const char* path)
    {
#ifdef _WIN32
        return SetCurrentDirectoryA(path);
#else
        return  0 == chdir(path);
#endif
    }
}
