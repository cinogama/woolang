#pragma once
#include "wo_compiler_lexer.hpp"
#include "wo_env_locale.hpp"

#include <string>
#include <sstream>
#include <fstream>
#include <unordered_map>
#include <shared_mutex>

#if WO_BUILD_WITH_MINGW
#   include <mingw.shared_mutex.h>
#endif

#include <optional>

namespace wo
{
    inline const std::wstring VIRTUAL_FILE_SCHEME = L"woovf://";
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
        const size_t shceme_length = VIRTUAL_FILE_SCHEME.size();
        if (uri.size() < shceme_length)
            return false;

        for (size_t i = 0; i < shceme_length; ++i)
        {
            if (uri[i] != VIRTUAL_FILE_SCHEME[i])
                return false;
        }
        return true;
    }

    inline bool check_virtual_file_path(
        std::wstring* out_real_read_path,
        const std::wstring& filepath,
        const std::optional<std::wstring>& script_path)
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
        if (script_path)
        {
            *out_real_read_path = wo::get_file_loc(script_path.value()) + L"/" + filepath;
            if (is_file_exist_and_readable(*out_real_read_path))
                return true;
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

                auto fnd = vfile_list.find(fullfilepath.substr(VIRTUAL_FILE_SCHEME.size()));
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
                auto fnd = vfile_list.find(fullfilepath.substr(VIRTUAL_FILE_SCHEME.size()));
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

                src_1->imbue(wo_global_locale);

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
        const std::optional<std::wstring>& script_path
    )
    {
        if (check_virtual_file_path(out_filefullpath, filepath, script_path))
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