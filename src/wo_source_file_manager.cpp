#include "wo_afx.hpp"

#include "wo_source_file_manager.hpp"
#include "wo_compiler_lexer.hpp"

#include "woort.h"

#include <istream>
#include <streambuf>

namespace wo
{
    std::optional<std::unique_ptr<std::istream>> open_virtual_file_stream(
        const std::string& fullfilepath)
    {
        woort_VFile* file = nullptr;

        if (woort_vfile_open(fullfilepath.c_str(), &file))
            return std::optional(
                std::unique_ptr<std::istream>(
                    std::make_unique<vfile_istream>(file)));

        return std::nullopt;
    }

    bool check_virtual_file_path(
        const std::string& filepath,
        const std::optional<const lexer*>& lex,
        std::string* out_real_read_path)
    {
        std::vector<std::string> search_dir_strings;
        std::vector<const char*> search_dirs;

        /* Walk the lexer import chain to collect source directories */
        auto finding_lex = lex;
        while (finding_lex.has_value())
        {
            auto* lex_instance = finding_lex.value();
            char* dir = woort_get_file_loc(
                lex_instance->get_source_path()->c_str());
            if (dir != nullptr)
            {
                search_dir_strings.emplace_back(dir);
                woort_free(dir);
            }
            finding_lex = lex_instance->get_who_import_me();
        }

        for (auto& dir : search_dir_strings)
            search_dirs.push_back(dir.c_str());

        char* resolved = nullptr;
        if (woort_vfs_resolve_path(
            filepath.c_str(),
            search_dirs.empty() ? nullptr : search_dirs.data(),
            search_dirs.size(),
            &resolved))
        {
            *out_real_read_path = resolved;
            woort_free(resolved);
            return true;
        }

        return false;
    }
}
