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

    bool check_virtual_file_path(
        const std::string& filepath,
        const std::optional<const lexer*>& lex,
        std::string* out_real_read_path);

    std::optional<std::unique_ptr<std::istream>> open_virtual_file_stream(
        const std::string& fullfilepath);
}
