#pragma once

#include <optional>
#include <string>
#include <tuple>

namespace utils::system {
    std::string formatSystemMessage(long errorCode);

    std::optional<std::string> deleteRegValue(const std::string&subKey, const std::string&valueName);

    std::optional<std::string> getEnvironmentVariable(const std::string&name);

    unsigned long getMainThreadId();

    std::string getModuleFileName(uint64_t moduleAddress);

    std::optional<std::string> getRegValue(const std::string&subKey, const std::string&valueName);

    std::string getSystemPath(const std::string&relativePath);

    std::string getUserName();

    std::tuple<int, int, int, int> getVersion();

    void setEnvironmentVariable(const std::string&name, const std::string&value = "");

    void setRegValue(const std::string&subKey, const std::string&valueName, const std::string&value);
}
