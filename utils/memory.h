#pragma once

#include <string>
#include <optional>

#include <models/MemoryAddress.h>
#include <types/SiVersion.h>

namespace utils::memory {
    models::MemoryAddress getAddresses(types::SiVersion::Full version);

    uint64_t scanPattern(const std::string& pattern);

    bool writeMemory(uint64_t address, const std::string& value);

    std::optional<uint32_t> readMemory32(uint64_t address, bool relative = true);
}
