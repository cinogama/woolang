#pragma once
#include "wo_env_locale.hpp"
#include "wo_utf8.hpp"

#include <map>
#include <string>
#include <sstream>
#include <fstream>
#include <unordered_map>
#include <shared_mutex>
#include <optional>
#include <cwchar>
#include <memory>

#if WO_BUILD_WITH_MINGW
#   include <mingw.shared_mutex.h>
#endif

namespace wo
{
    class lexer;

#define VIRTUAL_FILE_SCHEME_M "woovf://"
#define VIRTUAL_FILE_SCHEME_LEN 8
static_assert(
    sizeof(VIRTUAL_FILE_SCHEME_M) == VIRTUAL_FILE_SCHEME_LEN + 1);

    inline std::shared_mutex vfile_list_guard;

    struct vfile_information
    {
        bool enable_modify;
        std::string data;
    };

    inline std::map<std::string, vfile_information> vfile_list;

    inline void shutdown_virtual_binary()
    {
        vfile_list.clear();
    }

    inline bool create_virtual_binary(const std::string& filepath, const void* data, size_t length, bool enable_modify)
    {
        std::lock_guard g1(vfile_list_guard);
        if (auto vffnd = vfile_list.find(filepath);
            vffnd == vfile_list.end())
        {
            vfile_list.insert(
                std::make_pair(filepath, vfile_information{ 
                    enable_modify, 
                    std::string((const char*)data, length) }));
            return true;
        }
        else if (vffnd->second.enable_modify)
        {
            vffnd->second.enable_modify = enable_modify;
            vffnd->second.data = std::string((const char*)data, length);
            return true;
        }

        return false;
    }
    inline bool remove_virtual_binary(const std::string& filepath)
    {
        std::lock_guard g1(vfile_list_guard);
        if (auto vffnd = vfile_list.find(filepath);
            vffnd != vfile_list.end())
        {
            if (vffnd->second.enable_modify)
            {
                vfile_list.erase(vffnd);
                return true;
            }
        }
        return false;
    }

    inline bool is_virtual_uri(const std::string& uri)
    {
        return strncmp(uri.c_str(), VIRTUAL_FILE_SCHEME_M, VIRTUAL_FILE_SCHEME_LEN) == 0;
    }

    bool check_virtual_file_path(
        const std::string& filepath,
        const std::optional<const lexer*>& lex,
        std::string* out_real_read_path);

    inline std::optional<std::unique_ptr<std::istream>> open_virtual_file_stream(
        const std::string& fullfilepath)
    {
        // 1. Try exists file
        // 1) Read file from virtual file
        if (is_virtual_uri(fullfilepath))
        {
            std::shared_lock g1(vfile_list_guard);
            auto fnd = vfile_list.find(fullfilepath.substr(VIRTUAL_FILE_SCHEME_LEN));
            if (fnd != vfile_list.end())
                return std::optional(std::make_unique<std::istringstream>(fnd->second.data));
        }
        else
        {
            // 5) Read file from default path
            do
            {
                auto src_1 = std::make_unique<std::ifstream>(
                    fullfilepath,
                    std::ios_base::in | std::ios_base::binary);

                if (src_1->is_open())
                    return std::optional(std::move(src_1));

            } while (0);
        }

        return std::nullopt;
    }

    inline bool read_virtual_source(
        const std::string& fullfilepath,
        std::string* out_filecontent)
    {
        auto stream_may_null = open_virtual_file_stream(
            fullfilepath);

        if (stream_may_null)
        {
            auto& stream = stream_may_null.value();
            stream->seekg(0, std::ios::end);
            size_t len = (size_t)stream->tellg();
            stream->seekg(0, std::ios::beg);
            out_filecontent->resize(len, 0);
            stream->read(out_filecontent->data(), len);

            return true;
        }
        return false;
    }

    inline bool check_and_read_virtual_source(
        const std::string& filepath,
        const std::optional<const lexer*>& lex,
        std::string* out_filefullpath,
        std::string* out_filecontent)
    {
        if (check_virtual_file_path(filepath, lex, out_filefullpath))
            return read_virtual_source(*out_filefullpath, out_filecontent);
        return false;
    }

    inline std::vector<std::string> get_all_virtual_file_path()
    {
        std::shared_lock sg1(vfile_list_guard);
        std::vector<std::string> result;

        for (auto& [p, _] : vfile_list)
        {
            result.push_back(p);
        }
        return result;
    }
}