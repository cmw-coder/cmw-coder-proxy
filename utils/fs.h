#pragma once

#include <string>

namespace utils::fs {
    std::string readFile(const std::string& path);

    std::string getExtension(const std::string& path);

    std::filesystem::path getDirectory(const std::string& path);
}
