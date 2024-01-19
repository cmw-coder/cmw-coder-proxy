#pragma once

#include <string>

namespace utils::iconv {
    std::string gbkToUtf8(const std::string& source);

    std::string utf8ToGbk(const std::string& source);
}
