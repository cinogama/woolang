#pragma once

#include <string>
#include <optional>
#include <locale>
#include <vector>

namespace wo
{
    const std::locale& get_locale();
    const std::vector<std::string>& get_args();

    void wo_init_args(int argc, char** argv);
    void wo_init_locale();
    void wo_shutdown_locale_and_args();
   
    std::string get_file_loc(std::string path);

    std::string exe_path();
    void set_exe_path(const std::optional<std::string> path);

    std::string work_path();
    bool set_work_path(const std::string& path);
}
