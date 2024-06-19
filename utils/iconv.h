#pragma once

#include <filesystem>
#include <string>

namespace utils::iconv {
    std::string autoDecode(const std::string& source);

    std::string autoEncode(const std::string& source);

    std::filesystem::path toPath(const std::string& source);
}
