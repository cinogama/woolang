#include "wo_afx.hpp"

#include "wo_source_file_manager.hpp"
#include "wo_compiler_lexer.hpp"

#include "woort.h"

#include <istream>
#include <streambuf>

namespace wo
{
namespace
{

    class vfile_streambuf : public std::streambuf
    {
        woort_VFile* m_file;
        char         m_buf[1];

    public:
        explicit vfile_streambuf(woort_VFile* file) : m_file(file)
        {
            setg(nullptr, nullptr, nullptr);
        }

        ~vfile_streambuf() override
        {
            if (m_file != nullptr)
            {
                woort_vfile_close(m_file);
                m_file = nullptr;
            }
        }

        vfile_streambuf(const vfile_streambuf&) = delete;
        vfile_streambuf(vfile_streambuf&&) = delete;
        vfile_streambuf& operator = (const vfile_streambuf&) = delete;
        vfile_streambuf& operator = (vfile_streambuf&&) = delete;

    protected:
        int underflow() override
        {
            size_t n = 0;
            if (!woort_vfile_read(m_file, m_buf, 1, &n) || n == 0)
                return traits_type::eof();

            setg(m_buf, m_buf, m_buf + 1);
            return traits_type::to_int_type(m_buf[0]);
        }

        std::streamsize xsgetn(char* s, std::streamsize n) override
        {
            size_t read_bytes = 0;
            if (!woort_vfile_read(m_file, s, (size_t)n, &read_bytes))
                return 0;
            return (std::streamsize)read_bytes;
        }

        std::streampos seekoff(
            std::streamoff off,
            std::ios_base::seekdir dir,
            std::ios_base::openmode) override
        {
            int whence;
            switch (dir)
            {
            case std::ios_base::beg:  whence = SEEK_SET; break;
            case std::ios_base::cur:  whence = SEEK_CUR; break;
            case std::ios_base::end:  whence = SEEK_END; break;
            default:                  return std::streampos(-1);
            }

            if (!woort_vfile_seek(m_file, off, whence))
                return std::streampos(-1);

            setg(nullptr, nullptr, nullptr);
            return (std::streampos)woort_vfile_tell(m_file);
        }

        std::streampos seekpos(
            std::streampos pos,
            std::ios_base::openmode) override
        {
            if (!woort_vfile_seek(m_file, (int64_t)pos, SEEK_SET))
                return std::streampos(-1);

            setg(nullptr, nullptr, nullptr);
            return (std::streampos)woort_vfile_tell(m_file);
        }
    };

    struct vfile_istream : std::istream
    {
        vfile_streambuf m_buf;

        explicit vfile_istream(woort_VFile* file)
            : std::istream(nullptr)
            , m_buf(file)
        {
            rdbuf(&m_buf);
        }

        vfile_istream(const vfile_istream&) = delete;
        vfile_istream(vfile_istream&&) = delete;
        vfile_istream& operator = (const vfile_istream&) = delete;
        vfile_istream& operator = (vfile_istream&&) = delete;
    };

} // anonymous namespace

    bool is_file_exist_and_readable(const std::string& path)
    {
        return woort_fs_is_file_readable(path.c_str());
    }

    void shutdown_virtual_binary()
    {
        /* VFS lifecycle is managed by woort_init/woort_shutdown.
           This function is kept for backward compatibility. */
    }

    bool create_virtual_binary(const std::string& filepath, const void* data, size_t length, bool enable_modify)
    {
        return woort_vfs_create(filepath.c_str(), data, length, enable_modify);
    }

    bool remove_virtual_binary(const std::string& filepath)
    {
        return woort_vfs_remove(filepath.c_str());
    }

    bool is_virtual_uri(const std::string& uri)
    {
        return woort_vfs_is_virtual_uri(uri.c_str());
    }

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

    bool read_virtual_source(
        const std::string& fullfilepath,
        std::string* out_filecontent)
    {
        woort_VFile* file = nullptr;

        if (!woort_vfile_open(fullfilepath.c_str(), &file))
            return false;

        int64_t size = woort_vfile_size(file);
        if (size < 0)
        {
            woort_vfile_close(file);
            return false;
        }

        out_filecontent->resize((size_t)size, 0);

        size_t read_bytes = 0;
        bool ok = woort_vfile_read(
            file, out_filecontent->data(), (size_t)size, &read_bytes);

        woort_vfile_close(file);
        return ok && read_bytes == (size_t)size;
    }

    bool check_and_read_virtual_source(
        const std::string& filepath,
        const std::optional<const lexer*>& lex,
        std::string* out_filefullpath,
        std::string* out_filecontent)
    {
        if (check_virtual_file_path(filepath, lex, out_filefullpath))
            return read_virtual_source(*out_filefullpath, out_filecontent);
        return false;
    }

    std::vector<std::string> get_all_virtual_file_path()
    {
        std::vector<std::string> result;

        char** paths = nullptr;
        size_t count = woort_vfs_get_all_paths(&paths);

        for (size_t i = 0; i < count; ++i)
        {
            if (paths[i] != nullptr)
            {
                result.push_back(paths[i]);
                woort_free(paths[i]);
            }
        }

        woort_free(paths);
        return result;
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
