#pragma once

#include <string>
#include <tuple>

namespace utils::system {
    std::string formatSystemMessage(long errorCode);

    std::string getSystemPath(const std::string &relativePath);

    std::string getUserName();

    std::tuple<int, int, int, int> getVersion();

    unsigned long getMainThreadId();

    std::string getModuleFileName(uint64_t moduleAddress);

    void setRegValue(const std::string &subKey, const std::string &valueName, uint32_t value);

    void setRegValue(const std::string &subKey, const std::string &valueName, const std::string &value);

    std::string getRegValue(const std::string &subKey, const std::string &valueName);

    void deleteRegValue(const std::string &subKey, const std::string &valueName);
}