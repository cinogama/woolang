#include "wo_io.hpp"
#include "wo_assert.hpp"
#include "wo_utf8.hpp"

#include "wo_env_locale.hpp"
#include "wo_os_api.hpp"

#ifdef _WIN32
#       include <Windows.h>
#       undef max      // fucking windows.
#       undef min      // fucking windows.
#else
#       include <unistd.h>
#       include <sys/stat.h>
#endif

#include <iostream>
#include <memory>
#include <cstring>

#define WO_MAX_EXE_OR_RPATH_LEN 16384

namespace wo
{
    // ATTENTION:
    // RS will work in UTF-8 mode as default 
    // (Before Windows 10 build 17134, setlocale will not work when using UTF-8).
    inline std::locale wo_global_locale = std::locale::classic();
    inline std::string wo_global_locale_name = "";
    inline std::optional<std::wstring> wo_binary_path = std::nullopt;

    inline std::vector<std::string> wo_args;

    const std::locale& get_locale()
    {
        return wo_global_locale;
    }
    const std::vector<std::string>& get_args()
    {
        return wo_args;
    }

    void wo_init_args(int argc, char** argv)
    {
        wo_args.clear();
        for (int i = 0; i < argc; ++i)
            wo_args.push_back(argv[i]);
    }

    void wo_init_locale(const char* local_type)
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

    std::wstring get_file_loc(std::wstring path)
    {
        normalize_path(&path);

        size_t fnd = path.rfind(L'/');
        if (fnd < path.size())
            return path.substr(0, fnd);

        return L"";
    }

    std::wstring exe_path()
    {
        if (!wo_binary_path)
        {
            std::wstring result;
#ifdef _WIN32
            wchar_t _w_exe_path[WO_MAX_EXE_OR_RPATH_LEN] = {};
            const size_t len = (size_t)GetModuleFileNameW(NULL, _w_exe_path, WO_MAX_EXE_OR_RPATH_LEN);

            result.assign(_w_exe_path, len);
#else
            char _exe_path[WO_MAX_EXE_OR_RPATH_LEN] = {};
            const size_t len = (size_t)readlink("/proc/self/exe", _exe_path, WO_MAX_EXE_OR_RPATH_LEN);
            result = str_to_wstr(_exe_path);
#endif
            wo_test(len < WO_MAX_EXE_OR_RPATH_LEN);

            normalize_path(&result);

            wo_binary_path = get_file_loc(result);
        }
        return wo_binary_path.value();
    }
    void set_exe_path(const std::optional<std::wstring> path)
    {
        wo_binary_path = path;
    }

    std::wstring work_path()
    {
        std::wstring result;
#ifdef _WIN32
        wchar_t _w_exe_path[WO_MAX_EXE_OR_RPATH_LEN] = {};
        const size_t len = (size_t)GetCurrentDirectoryW(WO_MAX_EXE_OR_RPATH_LEN, _w_exe_path);
        
        wo_test(len < WO_MAX_EXE_OR_RPATH_LEN);

        result.assign(_w_exe_path, len);
#else
        char _exe_path[WO_MAX_EXE_OR_RPATH_LEN] = {};
        char* ptr = getcwd(_exe_path, WO_MAX_EXE_OR_RPATH_LEN);

        wo_test(ptr != nullptr);

        result = str_to_wstr(_exe_path);
#endif


        normalize_path(&result);

        return result;
    }
    bool set_work_path(const std::wstring& path)
    {
#ifdef _WIN32
        return (bool)SetCurrentDirectoryW(path.c_str());
#else
        return 0 == chdir(wstr_to_str(path).c_str());
#endif
    }
}