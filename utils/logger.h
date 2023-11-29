#pragma once

#include <string>

namespace utils::logger {
    void debug(const std::string&message);

    void error(const std::string&message);

    void info(const std::string&message);

    void log(const std::string&message);

    void warn(const std::string&message);
}
