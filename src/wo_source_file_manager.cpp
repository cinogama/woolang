#include "wo_source_file_manager.hpp"
#include "wo_compiler_lexer.hpp"
#include "wo_os_api.hpp"
#include "wo_utf8.hpp"

namespace wo
{
    bool check_virtual_file_path_impl(
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
        auto finding_lex = lex;
        while (finding_lex.has_value())
        {
            auto* lex_instance = finding_lex.value();

            *out_real_read_path =
                wo::get_file_loc(*lex_instance->get_source_path()) + L"/" + filepath;

            if (is_virtual_uri(*out_real_read_path))
            {
                // Virtual path, check it.
                std::shared_lock g1(vfile_list_guard);
                auto fnd = vfile_list.find(
                    out_real_read_path->substr(VIRTUAL_FILE_SCHEME_LEN));

                if (fnd != vfile_list.end())
                    return true;

            }
            else
            {
                // Not virtual path, check if file exist?
                if (is_file_exist_and_readable(*out_real_read_path))
                    return true;
            }
            finding_lex = lex_instance->get_who_import_me();
        }

        // 2) Read file from rpath
        do
        {
            *out_real_read_path = wo::work_path() + L"/" + filepath;
            if (is_file_exist_and_readable(*out_real_read_path))
                return true;
        } while (0);

        // 3) Read file from exepath
        do
        {
            *out_real_read_path = wo::exe_path() + L"/" + filepath;
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
            *out_real_read_path = VIRTUAL_FILE_SCHEME_W + filepath;

            std::shared_lock g1(vfile_list_guard);

            auto fnd = vfile_list.find(filepath);
            if (fnd != vfile_list.end())
                return true;

        } while (0);

        return false;
    }

    bool check_virtual_file_path(
        std::wstring* out_real_read_path,
        const std::wstring& filepath,
        const std::optional<const lexer*>& lex)
    {
        if (check_virtual_file_path_impl(out_real_read_path, filepath, lex))
        {
            normalize_path(out_real_read_path);
            return true;
        }
        return false;
    }
}