#pragma once

#include <string>

namespace utils::system {
    std::string formatSystemMessage(long errorCode);

    std::string getSystemPath(const std::string &relativePath);

    unsigned long getMainThreadId();

    std::string getModuleFileName(uint64_t moduleAddress);

    void setRegValue(const std::string &subKey, const std::string &valueName, uint32_t value);

    void setRegValue(const std::string &subKey, const std::string &valueName, const std::string &value);

    std::string getRegValue(const std::string &subKey, const std::string &valueName);
}