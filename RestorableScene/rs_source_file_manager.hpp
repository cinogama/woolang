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
    inline std::map<std::wstring, std::wstring> vfile_list;
    inline bool create_virtual_source(const std::wstring& file_data, const std::wstring& filepath)
    {
        std::lock_guard g1(vfile_list_guard);
        if (vfile_list.find(filepath) == vfile_list.end())
        {
            vfile_list[filepath] = file_data;
            return true;
        }

        return false;
    }
    inline bool read_virtual_source(std::wstring* out_result, std::wstring* out_real_read_path, const std::wstring& filepath, rs::lexer* lex = nullptr)
    {
        // 1. Try exists file
        // 1) Read file from script loc
        if (lex)
        {
            auto src_file_loc = rs::get_file_loc(lex->source_file);
            *out_real_read_path = str_to_wstr(src_file_loc) + filepath;
            std::wifstream src_1(*out_real_read_path);
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

        // 2) Read file from exepath
        do
        {
            *out_real_read_path = str_to_wstr(rs::exe_path()) + filepath;
            std::wifstream src_1(*out_real_read_path);
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
            *out_real_read_path = L"/virtual_files/" + filepath;
            std::shared_lock g1(vfile_list_guard);
            if (vfile_list.find(filepath) != vfile_list.end())
            {
                *out_result = vfile_list[filepath];
                return true;
            }
        } while (0);

        // 4) Read file from rpath
        do
        {
            *out_real_read_path = str_to_wstr(rs::work_path()) + filepath;
            std::wifstream src_1(*out_real_read_path);
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