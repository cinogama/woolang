#pragma once

#include "wo_io.hpp"
#include "wo_assert.hpp"
#include "wo_utf8.hpp"

#include <iostream>
#include <string>
#include <clocale>
#include <memory>
#include <cstring>
#include <vector>
#include <optional>

#ifdef _WIN32
#       include <Windows.h>
#       undef max      // fucking windows.
#       undef min      // fucking windows.
#else
#       include <unistd.h>
#       include <sys/stat.h>
#endif

namespace wo
{
    // ATTENTION:
    // RS will work in UTF-8 mode as default 
    // (Before Windows 10 build 17134, setlocale will not work when using UTF-8).

    inline std::locale wo_global_locale = std::locale::classic();
    inline std::string wo_global_locale_name = "";
    inline std::optional<std::string> wo_binary_path = std::nullopt;

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
#if !WO_BUILD_WITH_MINGW && defined(WO_NEED_ANSI_CONTROL) && defined(_WIN32)
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
        if (nullptr == std::setlocale(LC_CTYPE, local_type))
        {
            wo_warning("Unable to initialize locale character set environment: bad local type.");
        }
        else
        {
            wo_global_locale = std::locale(local_type);
            wo_global_locale_name = local_type;
        }

        if (wo::config::ENABLE_OUTPUT_ANSI_COLOR_CTRL)
            printf(ANSI_RST);
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
    inline std::wstring get_file_loc(std::wstring path)
    {
        for (auto& ch : path)
        {
            if (ch == L'\\')
                ch = L'/';
        }

        for (size_t index = path.size(); index > 0; index--)
            if (path[index - 1] == L'/')
                // find last '/' get substr and return..
                return path.substr(0, index - 1);

        return L"";
    }

    inline const char* exe_path()
    {
        if (wo_binary_path)
            return wo_binary_path.value().c_str();
        
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
    inline void set_exe_path(const char* path)
    {
        if (path != nullptr)
            wo_binary_path = std::optional(std::string(path));
        else
            wo_binary_path = std::nullopt;
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
