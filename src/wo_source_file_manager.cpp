#include "wo_source_file_manager.hpp"
#include "wo_compiler_lexer.hpp"

namespace wo
{
    bool check_virtual_file_path(
        std::wstring* out_real_read_path,
        const std::wstring& filepath,
        const std::optional<const lexer*>& lex)
    {
        if (is_virtual_uri(filepath))
        {
            *out_real_read_path = filepath;
            return true;
        }

        auto is_file_exist_and_readable =
            [](const std::wstring& path)
        {
            auto cpath = wstr_to_str(path);
#if WO_BUILD_WITH_MINGW
            FILE* f = fopen(cpath.c_str(), "r");
            if (f == nullptr)
                return false;

            fclose(f);
            return true;
#else
            struct stat file_stat;
            if (0 == stat(cpath.c_str(), &file_stat))
            {
                // Check if readable?
                return 0 == (file_stat.st_mode & S_IFDIR);
            }
            return false;
#endif
        };

        // 1. Try exists file
        // 1) Read file from script loc
        if (lex)
        {
            auto* finding_lex = lex.value();
            while (finding_lex != nullptr)
            {
                *out_real_read_path = wo::get_file_loc(finding_lex->script_path) + L"/" + filepath;
                if (is_file_exist_and_readable(*out_real_read_path))
                    return true;

                finding_lex = finding_lex->last_lexer;
            }
        }

        // 2) Read file from rpath
        do
        {
            *out_real_read_path = str_to_wstr(wo::work_path()) + L"/" + filepath;
            if (is_file_exist_and_readable(*out_real_read_path))
                return true;
        } while (0);

        // 3) Read file from exepath
        do
        {
            *out_real_read_path = str_to_wstr(wo::exe_path()) + L"/" + filepath;
            if (is_file_exist_and_readable(*out_real_read_path))
                return true;
        } while (0);

        // 4) Read file from default path
        do
        {
            *out_real_read_path = filepath;
            if (is_file_exist_and_readable(*out_real_read_path))
                return true;
        } while (0);

        // 5) Read file from virtual file
        do
        {
            *out_real_read_path = VIRTUAL_FILE_SCHEME + filepath;

            std::shared_lock g1(vfile_list_guard);
            auto fnd = vfile_list.find(filepath);
            if (fnd != vfile_list.end())
            {
                return true;
            }

        } while (0);

        return false;
    }
}