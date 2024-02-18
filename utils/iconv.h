#pragma once

#include <string>

#include <utils/system.h>

namespace utils::iconv {
    const auto needEncode = get<0>(system::getVersion()) == 3;

    std::string gbkToUtf8(const std::string& source);

    std::string utf8ToGbk(const std::string& source);
}
