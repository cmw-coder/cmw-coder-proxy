#pragma once

#include <string>
#include <optional>

namespace utils::memory {
    uint64_t scanPattern(const std::string &pattern);

    bool writeMemory(uint64_t address, const std::string &value);

    std::optional<uint32_t> readMemory32(uint64_t address, bool relative = true);
}