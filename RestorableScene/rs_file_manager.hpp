#pragma once
#include "rs_compiler_lexer.hpp"
#include "rs_env_locale.hpp"

#include <string>
#include <fstream>

namespace rs
{
    inline bool read_vfile(std::wstring * out_result, const std::wstring& filepath, rs::lexer* lex = nullptr)
    {
        // 1. Try exists file
        // 1) Read file from script loc
        if (lex)
        {
            auto src_file_loc = rs::get_file_loc(lex->source_file);
            std::wifstream src_1(str_to_wstr(src_file_loc) + filepath);
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
            auto src_file_loc = rs::exe_path();
            std::wifstream src_1(str_to_wstr(src_file_loc) + filepath);
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
        // TODO

        // 4) Read file from rpath
        do
        {
            auto src_file_loc = rs::work_path();
            std::wifstream src_1(str_to_wstr(src_file_loc) + filepath);
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