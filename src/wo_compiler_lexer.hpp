#pragma once
#include <fstream>

namespace wo
{
    struct source_base
    {
        source_base(const source_base&) = delete;
        source_base& operator = (const source_base&) = delete;

        virtual bool is_ready() const noexcept = 0;
    };

    struct text_source : public source_base
    {
        std::wstring m_source;
        text_source(const std::wstring& source)
            : m_source(source)
        {

        }
    };
    struct file_source : public source_base
    {
        std::wifstream m_file;
        file_source(const std::string& filepath)
            : m_file(filepath)
        {

        }
    };

    class lexer
    {
        lexer(const lexer&) = delete;
        lexer& operator = (const lexer&) = delete;
        lexer(lexer&&) = delete;
        lexer& operator = (lexer&&) = delete;

        source_base* m_source;

        ~lexer()
        {

        }

        template<typename SourceT>
        lexer(SourceT&& src)
            : m_source(new SourceT(std::move(src)))
        {
            if ()
        }

    public:

        template<typename SourceT>
        lexer* create()
        {

        }
    };
}