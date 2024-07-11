#pragma once

#include <string>

namespace utils::fs {
    std::string readFile(const std::string& path);

    std::string readFile(const std::string& path, uint32_t startLine, uint32_t endLine);
}
