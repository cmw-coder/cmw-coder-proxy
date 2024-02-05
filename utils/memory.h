#pragma once

namespace utils::memory {
    uint32_t offset(uint32_t offset);

    template<typename T>
    void read(const uint32_t address, T& value) {
        memcpy(&value, reinterpret_cast<void const *>(address), sizeof(T));
    }

    template<typename T>
    uint32_t search(const T& input, const uint32_t address = offset(0), const uint32_t maxSize = 100000) {
        const auto baseAddress = reinterpret_cast<uint8_t *>(address);
        for (uint32_t offset = 0; offset < maxSize; offset++) {
            if (memcmp(baseAddress + offset, &input, sizeof(T)) == 0) {
                return address + offset;
            }
        }
        return {};
    }

    template<typename T>
    void write(const uint32_t address, const T& value) {
        memcpy(reinterpret_cast<void *>(address), &value, sizeof(T));
    }
}
