#include "wo_afx.hpp"

#include "wo_env_locale.hpp"

#ifdef _WIN32
#       include <Windows.h>
#       undef max      // fucking windows.
#       undef min      // fucking windows.
#else
#       include <unistd.h>
#       include <sys/stat.h>
#endif


#define WO_MAX_EXE_OR_RPATH_LEN 16384

namespace wo
{
    // ATTENTION:
    // Woolang will work in UTF-8 mode as default 

#ifdef _WIN32
    const char* DEFAULT_LOCALE_NAME = ".UTF-8";
#else
    const char* DEFAULT_LOCALE_NAME = "C.UTF-8";
#endif

    inline std::locale wo_global_locale = std::locale::classic();
    inline std::string wo_global_locale_name = "";
    inline std::optional<std::string> wo_binary_path = std::nullopt;
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
    void wo_init_locale()
    {
        if (nullptr == std::setlocale(LC_CTYPE, DEFAULT_LOCALE_NAME))
        {
            wo_warning("Unable to initialize locale character set environment: bad local type.");
        }
        else
        {
            wo_global_locale = std::locale(DEFAULT_LOCALE_NAME);
            wo_global_locale_name = DEFAULT_LOCALE_NAME;
        }

#ifdef _WIN32
        // SUPPORT ANSI_CONTROL
#if !WO_BUILD_WITH_MINGW && defined(WO_NEED_ANSI_CONTROL)
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
        // Set console input & output and using UTF8.
        SetConsoleCP(CP_UTF8);
        SetConsoleOutputCP(CP_UTF8);
#endif

        if (wo::config::ENABLE_OUTPUT_ANSI_COLOR_CTRL)
            printf(ANSI_RST);
    }
    void wo_shutdown_locale_and_args()
    {
        wo_binary_path.reset();
        wo_args.clear();
    }

    std::string get_file_loc(std::string path)
    {
        normalize_path(&path);

        size_t fnd = path.rfind('/');
        if (fnd < path.size())
            return path.substr(0, fnd);

        return "";
    }
    std::string exe_path()
    {
        if (!wo_binary_path)
        {
            std::string result;
#if WO_DISABLE_FUNCTION_FOR_WASM
#else
#   ifdef _WIN32
            wchar_t _w_exe_path[WO_MAX_EXE_OR_RPATH_LEN] = {};
            const size_t len = (size_t)GetModuleFileNameW(NULL, _w_exe_path, WO_MAX_EXE_OR_RPATH_LEN);
            wo_test(len < WO_MAX_EXE_OR_RPATH_LEN);

            static_assert(sizeof(wchar_t) == sizeof(char16_t));
            result = wo::u16strtou8(
                reinterpret_cast<const char16_t*>(_w_exe_path),
                len);
#   else
            char _exe_path[WO_MAX_EXE_OR_RPATH_LEN] = {};
            const size_t len = (size_t)readlink("/proc/self/exe", _exe_path, WO_MAX_EXE_OR_RPATH_LEN);
            wo_test(len < WO_MAX_EXE_OR_RPATH_LEN);

            result = _exe_path;
#   endif
#endif
            normalize_path(&result);

            wo_binary_path = get_file_loc(result);
        }
        return wo_binary_path.value();
    }
    void set_exe_path(const std::optional<std::string> path)
    {
        wo_binary_path = path;
    }

    std::string work_path()
    {
        std::string result;
#if WO_DISABLE_FUNCTION_FOR_WASM
#else
#   ifdef _WIN32
        wchar_t _w_work_path[WO_MAX_EXE_OR_RPATH_LEN] = {};
        const size_t len = (size_t)GetCurrentDirectoryW(WO_MAX_EXE_OR_RPATH_LEN, _w_work_path);

        wo_test(len < WO_MAX_EXE_OR_RPATH_LEN);

        static_assert(sizeof(wchar_t) == sizeof(char16_t));
        result = wo::u16strtou8(
            reinterpret_cast<const char16_t*>(_w_work_path),
            len);
#   else
        char _work_path[WO_MAX_EXE_OR_RPATH_LEN] = {};
        char* ptr = getcwd(_work_path, WO_MAX_EXE_OR_RPATH_LEN);

        wo_test(ptr != nullptr);
        result = _work_path;
#   endif
#endif
        normalize_path(&result);

        return result;
    }
    bool set_work_path(const std::string& path)
    {
#if WO_DISABLE_FUNCTION_FOR_WASM
        return false;
#else
#   ifdef _WIN32
        auto wstr = wo::u8strtou16(path.data(), path.size());
        static_assert(sizeof(wchar_t) == sizeof(char16_t));

        return (bool)SetCurrentDirectoryW(reinterpret_cast<const wchar_t*>(wstr.c_str()));
#   else
        return 0 == chdir(path.c_str());
#   endif
#endif
    }
}
