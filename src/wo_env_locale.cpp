#include "wo_afx.hpp"

#include "wo_env_locale.hpp"

#ifdef _WIN32
#   include <Windows.h>
#   undef max      // fucking windows.
#   undef min      // fucking windows.
#else
#   include <unistd.h>
#   include <sys/stat.h>
#   ifdef __APPLE__
#       include <mach-o/dyld.h>
#   endif
#endif


#define WO_MAX_EXE_OR_RPATH_LEN 16384

namespace wo
{
    // ATTENTION:
    // Woolang will work in UTF-8 mode as default 

#ifdef _WIN32
    static const char* DEFAULT_LOCALE_NAME = ".UTF-8";

    class cin_win32_u16_to_u8 : public std::streambuf
    {
        static constexpr size_t BUFFER_LIMIT = 16384;

        bool m_eof_flag;

        size_t m_u16exract_place;
        size_t m_u16readable_length;

        char m_u8buffer[BUFFER_LIMIT];
        wchar_t m_u16buffer[BUFFER_LIMIT];

    public:
        cin_win32_u16_to_u8()
            : m_eof_flag(false)
            , m_u16exract_place(BUFFER_LIMIT)
            , m_u16readable_length(0)
        {
        }

        virtual int_type underflow() override
        {
            if (m_eof_flag)
                return traits_type::eof();

            if (gptr() < egptr())
                return traits_type::to_int_type(*gptr());

            if (m_u16exract_place == BUFFER_LIMIT)
            {
                DWORD readed_u16count;
                if (FALSE == ReadConsoleW(
                    GetStdHandle(STD_INPUT_HANDLE),
                    m_u16buffer,
                    BUFFER_LIMIT,
                    &readed_u16count,
                    NULL) || readed_u16count == 0)
                {
                    // Ctrl+C pressed?
                    auto err = GetLastError();
                    switch (err)
                    {
                    case ERROR_OPERATION_ABORTED:
                        // Ctrl+C pressed, skip this round.
                        readed_u16count = 0;
                        break;
                    default:
                        m_eof_flag = true;
                        return traits_type::eof();
                    }
                }
                m_u16exract_place = 0;
                m_u16readable_length = static_cast<size_t>(readed_u16count);
            }

            // Convert u16 serial to u8 serial;
            size_t u8_buffer_next_place = 0;
            while (m_u16exract_place < m_u16readable_length)
            {
                size_t u8len;
                char u8buf[UTF8MAXLEN];

                static_assert(sizeof(wchar_t) == sizeof(char16_t));
                const size_t u16forward = u16exractu8(
                    reinterpret_cast<const char16_t*>(m_u16buffer + m_u16exract_place),
                    m_u16readable_length - m_u16exract_place,
                    u8buf,
                    &u8len);

                if (u8_buffer_next_place + u8len > BUFFER_LIMIT)
                {
                    wo_assert(u8_buffer_next_place != 0);

                    // Not enough to store the u8 char, keep u16 state.
                    setg(m_u8buffer, m_u8buffer, m_u8buffer + u8_buffer_next_place);
                    return traits_type::to_int_type(*this->gptr());
                }

                // Ignore `\r` from `\r\n`
                if (u8len != 1 || u8buf[0] != '\r')
                {
                    memcpy(m_u8buffer + u8_buffer_next_place, u8buf, u8len);
                    u8_buffer_next_place += u8len;
                }
                m_u16exract_place += u16forward;
            }

            // Read finished.
            m_u16readable_length = 0;
            m_u16exract_place = BUFFER_LIMIT;

            setg(m_u8buffer, m_u8buffer, m_u8buffer + u8_buffer_next_place);
            return traits_type::to_int_type(*this->gptr());
        }
    };

    std::streambuf* _win32_origin_cin_buf;
#elif defined(__APPLE__)
    static const char* DEFAULT_LOCALE_NAME = "en_US.UTF-8";
#else
    static const char* DEFAULT_LOCALE_NAME = "C.UTF-8";
#endif

    static std::locale wo_global_locale = std::locale::classic();
    static std::string wo_global_locale_name = "";
    static std::optional<std::string> wo_binary_path = std::nullopt;
    static std::vector<std::string> wo_args;

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
#ifdef _WIN32
        // SUPPORT ANSI_CONTROL
#if defined(WO_NEED_ANSI_CONTROL) && defined(ENABLE_VIRTUAL_TERMINAL_PROCESSING)
        auto this_console_handle = GetStdHandle(STD_OUTPUT_HANDLE);
        if (this_console_handle != INVALID_HANDLE_VALUE)
        {
            DWORD console_mode = 0;
            if (GetConsoleMode(
                this_console_handle, &console_mode))
            {
                SetConsoleMode(
                    this_console_handle, 
                    console_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
            }
        }
#endif
        wo_assert(_win32_origin_cin_buf == nullptr);
        _win32_origin_cin_buf = std::cin.rdbuf(new cin_win32_u16_to_u8());
#endif
        if (nullptr == std::setlocale(LC_CTYPE, DEFAULT_LOCALE_NAME))
            wo_warning("Unable to initialize locale character set environment: bad local type.");
        else
        {
            wo_global_locale = std::locale(DEFAULT_LOCALE_NAME);
            wo_global_locale_name = DEFAULT_LOCALE_NAME;
        }

        if (wo::config::ENABLE_OUTPUT_ANSI_COLOR_CTRL)
            printf(ANSI_RST);

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
#   elif defined(__APPLE__)
        char _exe_path[WO_MAX_EXE_OR_RPATH_LEN] = {};
        uint32_t size = WO_MAX_EXE_OR_RPATH_LEN;
        if (_NSGetExecutablePath(_exe_path, &size) == 0)
        {
            char resolved_path[WO_MAX_EXE_OR_RPATH_LEN] = {};
            if (realpath(_exe_path, resolved_path) != nullptr)
                result = resolved_path;
            else
                result = _exe_path;
        }
        wo_test(!result.empty());
#   else
        char _exe_path[WO_MAX_EXE_OR_RPATH_LEN] = {};
        const size_t len = (size_t)readlink("/proc/self/exe", _exe_path, WO_MAX_EXE_OR_RPATH_LEN);
        wo_test(len < WO_MAX_EXE_OR_RPATH_LEN);

        result = _exe_path;
#   endif
#endif
        normalize_path(&result);

        // Fetch & set host path.
        wo_binary_path = get_file_loc(result);
    }
    void wo_shutdown_locale_and_args()
    {
        wo_binary_path.reset();
        wo_args.clear();

#ifdef _WIN32
        delete std::cin.rdbuf(_win32_origin_cin_buf);
        _win32_origin_cin_buf = nullptr;
#endif
        // Reset host path.
        wo_binary_path.reset();
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
