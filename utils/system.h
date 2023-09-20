#pragma once

#include <optional>

namespace utils::system {
    std::string getSystemDirectory();

    unsigned long getMainThreadId();

    std::string getModuleFileName(uint64_t moduleAddress);

    uint64_t scanPattern(const std::string &pattern);

    bool writeMemory(uint64_t address, const std::string &value);

    std::optional<uint32_t> readMemory32(uint64_t address, bool relative = true);

    void setRegValue32(
            const std::string &subKey,
            const std::string &valueName,
            uint32_t value
    );
}