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

#if WO_BUILD_WITH_MINGW
#   include <mingw.shared_mutex.h>
#endif

namespace wo
{
    class lexer;

#define VIRTUAL_FILE_SCHEME_W L"woovf://"
#define VIRTUAL_FILE_SCHEME_M "woovf://"
#define VIRTUAL_FILE_SCHEME_LEN 8
static_assert(
    sizeof(VIRTUAL_FILE_SCHEME_M) == sizeof(VIRTUAL_FILE_SCHEME_W) / sizeof(wchar_t)
    && sizeof(VIRTUAL_FILE_SCHEME_M) == VIRTUAL_FILE_SCHEME_LEN + 1);

    inline std::shared_mutex vfile_list_guard;

    struct vfile_information
    {
        bool enable_modify;
        bool has_width_data;
        std::string data;
        std::wstring wdata;
    };

    inline std::map<std::wstring, vfile_information> vfile_list;

    inline bool create_virtual_binary(const std::wstring& filepath, const void* data, size_t length, bool enable_modify)
    {
        std::lock_guard g1(vfile_list_guard);
        if (auto vffnd = vfile_list.find(filepath);
            vffnd == vfile_list.end())
        {
            vfile_list[filepath] = { enable_modify, false, std::string((const char*)data, length) };
            return true;
        }
        else if (vffnd->second.enable_modify)
        {
            vfile_list[filepath] = { enable_modify, false, std::string((const char*)data, length) };
            return true;
        }

        return false;
    }
    inline bool remove_virtual_binary(const std::wstring& filepath)
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

    inline bool is_virtual_uri(const std::wstring& uri)
    {
        return wcsncmp(uri.c_str(), VIRTUAL_FILE_SCHEME_W, VIRTUAL_FILE_SCHEME_LEN) == 0;
    }

    bool check_virtual_file_path(
        std::wstring* out_real_read_path,
        const std::wstring& filepath,
        const std::optional<const lexer*>& lex);

    // NOTE: Remember to free!
    template<bool width = true>
    inline auto open_virtual_file_stream(
        const std::wstring& fullfilepath
    ) -> std::optional<std::unique_ptr<typename stream_types<width>::stream>>
    {
        using fstream_t = typename stream_types<width>::ifile_stream;
        using sstream_t = typename stream_types<width>::istring_stream;

        // 1. Try exists file
        // 1) Read file from virtual file
        if (is_virtual_uri(fullfilepath))
        {
            if constexpr (width)
            {
                std::lock_guard g1(vfile_list_guard);

                auto fnd = vfile_list.find(fullfilepath.substr(VIRTUAL_FILE_SCHEME_LEN));
                if (fnd != vfile_list.end())
                {
                    if (fnd->second.has_width_data == false)
                    {
                        fnd->second.has_width_data = true;
                        fnd->second.wdata = str_to_wstr(fnd->second.data);
                    }
                    return std::optional(std::make_unique<sstream_t>(fnd->second.wdata));
                }
            }
            else
            {
                std::shared_lock g1(vfile_list_guard);
                auto fnd = vfile_list.find(fullfilepath.substr(VIRTUAL_FILE_SCHEME_LEN));
                if (fnd != vfile_list.end())
                    return std::optional(std::make_unique<sstream_t>(fnd->second.data));
            }
        }
        else
        {
            // 5) Read file from default path
            do
            {
                auto src_1 = std::make_unique<fstream_t>(
                    wstr_to_str(fullfilepath),
                    std::ios_base::in | std::ios_base::binary);

                src_1->imbue(wo::get_locale());

                if (src_1->is_open())
                    return std::optional(std::move(src_1));

            } while (0);
        }

        return std::nullopt;
    }

    template<bool width = true>
    inline bool read_virtual_source(
        typename stream_types<width>::buffer* out_filecontent,
        const std::wstring& fullfilepath)
    {
        auto stream_may_null = open_virtual_file_stream<width>(
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

    template<bool width = true>
    inline bool check_and_read_virtual_source(
        typename stream_types<width>::buffer* out_filecontent,
        std::wstring* out_filefullpath,
        const std::wstring& filepath,
        const std::optional<const lexer*>& lex
    )
    {
        if (check_virtual_file_path(out_filefullpath, filepath, lex))
            return read_virtual_source<width>(out_filecontent, *out_filefullpath);
        return false;
    }

    inline std::vector<std::wstring> get_all_virtual_file_path()
    {
        std::shared_lock sg1(vfile_list_guard);
        std::vector<std::wstring> result;

        for (auto& [p, _] : vfile_list)
        {
            result.push_back(p);
        }
        return result;
    }
}