#pragma once

namespace utils::system {
    std::string getSystemDirectory();

    unsigned long getMainThreadId();

    std::string getModuleFileName(uint64_t moduleAddress);

    uint64_t scanPattern(const std::string &pattern);

    bool writeMemory(uint64_t address, const std::string &value);

    void setRegValue32(
            const std::string &subKey,
            const std::string &valueName,
            uint32_t value
    );
}