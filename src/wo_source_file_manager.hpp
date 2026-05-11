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

    void shutdown_virtual_binary();

    bool create_virtual_binary(const std::string& filepath, const void* data, size_t length, bool enable_modify);
    bool remove_virtual_binary(const std::string& filepath);

    bool is_virtual_uri(const std::string& uri);

    bool check_virtual_file_path(
        const std::string& filepath,
        const std::optional<const lexer*>& lex,
        std::string* out_real_read_path);

    std::optional<std::unique_ptr<std::istream>> open_virtual_file_stream(
        const std::string& fullfilepath);

    bool read_virtual_source(
        const std::string& fullfilepath,
        std::string* out_filecontent);

    bool check_and_read_virtual_source(
        const std::string& filepath,
        const std::optional<const lexer*>& lex,
        std::string* out_filefullpath,
        std::string* out_filecontent);

    std::vector<std::string> get_all_virtual_file_path();
}
