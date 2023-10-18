#pragma once

#include <string>

namespace utils::crypto {
    enum class Encoding {
        Base64,
        Hex
    };

    std::string encode(const std::string &input, Encoding encoding);

    std::string decode(const std::string &input, Encoding encoding);

    std::string sha1(const std::string &input, Encoding encoding = Encoding::Hex);
}