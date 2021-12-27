#pragma once
#include "rs_compiler_lexer.hpp"
#include "rs_env_locale.hpp"

#include <string>
#include <fstream>
#include <unordered_map>
#include <shared_mutex>

namespace rs
{
    inline std::shared_mutex vfile_list_guard;

    struct vfile_information
    {
        bool enable_modify;
        std::wstring data;
    };

    inline std::map<std::wstring, vfile_information> vfile_list;
    inline bool create_virtual_source(const std::wstring& file_data, const std::wstring& filepath, bool enable_modify)
    {
        std::lock_guard g1(vfile_list_guard);
        if (auto vffnd = vfile_list.find(filepath);
            vffnd == vfile_list.end())
        {
            vfile_list[filepath] = { enable_modify, file_data };
            return true;
        }
        else if (vffnd->second.enable_modify)
        {
            vfile_list[filepath] = { enable_modify, file_data };
            return true;
        }

        return false;
    }

    template<typename LEXER = void>
    inline bool read_virtual_source(std::wstring* out_result, std::wstring* out_real_read_path, const std::wstring& filepath, LEXER* lex = nullptr)
    {
        // 1. Try exists file
        // 1) Read file from script loc
        if constexpr (!std::is_same<LEXER, void>::value)
        {
            if (lex)
            {
                auto src_file_loc = rs::get_file_loc(lex->source_file);
                *out_real_read_path = str_to_wstr(src_file_loc) + filepath;
                std::wifstream src_1(wstr_to_str(*out_real_read_path));

                src_1.imbue(rs_global_locale);

                if (src_1.is_open())
                {
                    src_1.seekg(0, std::ios::end);
                    auto len = src_1.tellg();
                    src_1.seekg(0, std::ios::beg);
                    out_result->resize(len, 0);
                    src_1.read(out_result->data(), len);
                    return true;
                }
            }
        }

        // 2) Read file from exepath
        do
        {
            *out_real_read_path = str_to_wstr(rs::exe_path()) + filepath;
            std::wifstream src_1(wstr_to_str(*out_real_read_path));

            src_1.imbue(rs_global_locale);

            if (src_1.is_open())
            {
                src_1.seekg(0, std::ios::end);
                auto len = src_1.tellg();
                src_1.seekg(0, std::ios::beg);
                out_result->resize(len, 0);
                src_1.read(out_result->data(), len);
                return true;
            }

        } while (0);

        // 3) Read file from virtual file
        do
        {
            *out_real_read_path = filepath;
            std::shared_lock g1(vfile_list_guard);
            if (vfile_list.find(filepath) != vfile_list.end())
            {
                *out_result = vfile_list[filepath].data;
                return true;
            }
        } while (0);

        // 4) Read file from rpath
        do
        {
            *out_real_read_path = str_to_wstr(rs::work_path()) + filepath;
            std::wifstream src_1(wstr_to_str(*out_real_read_path));

            src_1.imbue(rs_global_locale);

            if (src_1.is_open())
            {
                src_1.seekg(0, std::ios::end);
                auto len = src_1.tellg();
                src_1.seekg(0, std::ios::beg);
                out_result->resize(len, 0);
                src_1.read(out_result->data(), len);
                return true;
            }

        } while (0);

        // 5) Read file from default path
        do
        {
            std::wifstream src_1(filepath);

            src_1.imbue(rs_global_locale);

            if (src_1.is_open())
            {
                src_1.seekg(0, std::ios::end);
                auto len = src_1.tellg();
                src_1.seekg(0, std::ios::beg);
                out_result->resize(len, 0);
                src_1.read(out_result->data(), len);
                return true;
            }

        } while (0);

        return false;
    }
}