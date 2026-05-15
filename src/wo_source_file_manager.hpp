#pragma once
#include "wo_utf8.hpp"

#include <string>
#include <sstream>
#include <fstream>
#include <optional>
#include <vector>
#include <memory>

namespace wo
{
    class lexer;

    struct vfile_istream : std::istream
    {
        class vfile_streambuf : public std::streambuf
        {
            static constexpr size_t BUF_SIZE = 4096;

            woort_VFile* m_file;
            char         m_buf[BUF_SIZE];

        public:
            explicit vfile_streambuf(woort_VFile* file) : m_file(file)
            {
                setg(m_buf, m_buf + BUF_SIZE, m_buf + BUF_SIZE);
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
                if (gptr() < egptr())
                    return traits_type::to_int_type(*gptr());

                size_t nread = woort_vfile_read(m_file, m_buf, BUF_SIZE);
                if (nread == 0)
                    return traits_type::eof();

                setg(m_buf, m_buf, m_buf + nread);
                return traits_type::to_int_type(m_buf[0]);
            }

            std::streamsize xsgetn(char* s, std::streamsize n) override
            {
                std::streamsize total = 0;
                std::streamsize avail = egptr() - gptr();

                if (avail > 0)
                {
                    std::streamsize count = avail < n ? avail : n;
                    std::memcpy(s, gptr(), (size_t)count);
                    gbump((int)count);
                    total += count;
                    if (count >= n)
                        return total;
                    s += count;
                    n -= count;
                }

                total += (std::streamsize)woort_vfile_read(m_file, s, (size_t)n);
                return total;
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

                setg(m_buf, m_buf + BUF_SIZE, m_buf + BUF_SIZE);
                return (std::streampos)woort_vfile_tell(m_file);
            }

            std::streampos seekpos(
                std::streampos pos,
                std::ios_base::openmode) override
            {
                if (!woort_vfile_seek(m_file, (int64_t)pos, SEEK_SET))
                    return std::streampos(-1);

                setg(m_buf, m_buf + BUF_SIZE, m_buf + BUF_SIZE);
                return (std::streampos)woort_vfile_tell(m_file);
            }
        };


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

    bool check_virtual_file_path(
        const std::string& filepath,
        const std::optional<const lexer*>& lex,
        std::string* out_real_read_path);

    std::optional<std::unique_ptr<std::istream>> open_virtual_file_stream(
        const std::string& fullfilepath);
}
