#pragma once

#include <string>
#include <optional>
#include <locale>
#include <vector>

namespace wo
{
    extern const char* DEFAULT_LOCALE_NAME;

    const std::locale& get_locale();
    const std::vector<std::string>& get_args();

    void wo_init_args(int argc, char** argv);
    void wo_init_locale(const char* local_type);
   
    std::wstring get_file_loc(std::wstring path);

    std::wstring exe_path();
    void set_exe_path(const std::optional<std::wstring> path);

    std::wstring work_path();
    bool set_work_path(const std::wstring& path);
}
