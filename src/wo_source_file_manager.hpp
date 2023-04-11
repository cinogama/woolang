#pragma once
#include "wo_compiler_lexer.hpp"
#include "wo_env_locale.hpp"

#include <string>
#include <sstream>
#include <fstream>
#include <unordered_map>
#include <shared_mutex>

namespace wo
{
    inline std::shared_mutex vfile_list_guard;

    struct vfile_information
    {
        bool enable_modify;
        bool has_width_data;
        std::string data;
        std::wstring wdata;
    };

    inline std::map<std::wstring, vfile_information> vfile_list;

    inline bool create_virtual_binary(const char* data, size_t length, const std::wstring& filepath, bool enable_modify)
    {
        std::lock_guard g1(vfile_list_guard);
        if (auto vffnd = vfile_list.find(filepath);
            vffnd == vfile_list.end())
        {
            vfile_list[filepath] = { enable_modify, false, std::string(data, length) };
            return true;
        }
        else if (vffnd->second.enable_modify)
        {
            vfile_list[filepath] = { enable_modify, false, std::string(data, length) };
            return true;
        }

        return false;
    }

    template<bool width>
    struct stream_types
    {
        using stream = std::istream;
        using ifile_stream = std::ifstream;
        using istring_stream = std::istringstream;
        using buffer = std::string;
    };

    template<>
    struct stream_types<true>
    {
        using stream = std::wistream;
        using ifile_stream = std::wifstream;
        using istring_stream = std::wistringstream;
        using buffer = std::wstring;
    };

    // NOTE: Remember to free!
    template<bool width = true>
    inline auto open_virtual_file_stream(std::wstring* out_real_read_path, const std::wstring& filepath, const char* script_file)
        -> typename stream_types<width>::stream*
    {
        using fstream_t = typename stream_types<width>::ifile_stream;
        using sstream_t = typename stream_types<width>::istring_stream;

        // 1. Try exists file
        // 1) Read file from script loc
        if (script_file)
        {
            auto src_file_loc = wo::get_file_loc(script_file);
            *out_real_read_path = str_to_wstr(src_file_loc) + filepath;
            auto* src_1 = new fstream_t(wstr_to_str(*out_real_read_path), std::ios_base::in | std::ios_base::binary);

            src_1->imbue(wo_global_locale);

            if (src_1->is_open())
                return src_1;

            delete src_1;
        }

        // 2) Read file from exepath
        do
        {
            *out_real_read_path = str_to_wstr(wo::exe_path()) + filepath;
            auto* src_1 = new fstream_t(wstr_to_str(*out_real_read_path), std::ios_base::in | std::ios_base::binary);

            src_1->imbue(wo_global_locale);

            if (src_1->is_open())
                return src_1;

            delete src_1;
        } while (0);

        // 3) Read file from virtual file
        do
        {
            *out_real_read_path = filepath;

            if constexpr (width)
            {
                std::lock_guard g1(vfile_list_guard);

                auto fnd = vfile_list.find(filepath);
                if (fnd != vfile_list.end())
                {
                    if (fnd->second.has_width_data == false)
                    {
                        fnd->second.has_width_data = true;
                        fnd->second.wdata = str_to_wstr(fnd->second.data);
                    }
                    return new sstream_t(fnd->second.wdata);
                }
            }
            else
            {
                std::shared_lock g1(vfile_list_guard);

                auto fnd = vfile_list.find(filepath);
                if (fnd != vfile_list.end())
                    return new sstream_t(fnd->second.data);
            }

        } while (0);

        // 4) Read file from rpath
        do
        {
            *out_real_read_path = str_to_wstr(wo::work_path()) + filepath;
            auto* src_1 = new fstream_t(wstr_to_str(*out_real_read_path), std::ios_base::in | std::ios_base::binary);

            src_1->imbue(wo_global_locale);

            if (src_1->is_open())
                return src_1;

            delete src_1;

        } while (0);

        // 5) Read file from default path
        do
        {
            *out_real_read_path = filepath;
            auto* src_1 = new fstream_t(wstr_to_str(filepath), std::ios_base::in | std::ios_base::binary);

            src_1->imbue(wo_global_locale);

            if (src_1->is_open())
                return src_1;

            delete src_1;

        } while (0);

        return nullptr;
    }

    template<bool width = true, typename LEXER = void>
    inline bool read_virtual_source(typename stream_types<width>::buffer* out_result, std::wstring* out_real_read_path, const std::wstring& filepath, const char* script_file)
    {
        auto* stream = open_virtual_file_stream<width>(out_real_read_path, filepath, script_file);
        if (stream)
        {
            stream->seekg(0, std::ios::end);
            auto len = stream->tellg();
            stream->seekg(0, std::ios::beg);
            out_result->resize(len, 0);
            stream->read(out_result->data(), len);

            delete stream;
            return true;
        }
        return false;
    }
}