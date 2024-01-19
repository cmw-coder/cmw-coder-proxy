#include <format>
#include <memory>
#include <stdexcept>

#include <utils/memory.h>

#include <windows.h>

using namespace std;
using namespace types;
using namespace utils;

namespace {
    const SiVersion::Map<MemoryAddress> addressMap = {
        {
            SiVersion::Major::V35, {
                {
                    SiVersion::Minor::V0086, {
                        {
                            0x1C35A4,
                            {0x08DB9B},
                            {0x08F533},
                            {0x0C93D8, 0x000010, 0x000000},
                            {0x08D530},
                            {0x08D474},
                        },
                        {
                            0x1C7C00,
                            {0x000000, 0x000044},
                        },
                        {
                            0x1CCD44,
                            {0x1BE0CC, 0x1C574C, 0x1E3B9C, 0x1E3BA4},
                            {0x000024},
                            {0x1CCD3C},
                            {0x09180C, 0X1CCD48},
                            {0x108894},
                            {0x1007D7},
                        }
                    },
                },
            },
        },
        {
            SiVersion::Major::V40, {
                {
                    SiVersion::Minor::V0128, {
                        {
                            0x26D484,
                            {0x08DB9B},
                            {0x0C6660},
                            {0x0419A0, 0x000010, 0x000000},
                            {0x0C4A60},
                            {0x0C4FB0},
                        },
                        {
                            0x288F30,
                            {0x25A9B4, 0x28A0FC},
                            {0x25A9B4, 0x28A0FC, 0x26DAE0, 0x26DAE8},
                            {0x000024},
                            {0x288F48},
                            {0x0C88C0, 0X288F2C},
                            {0x187660},
                            {0x179D30},
                        }
                    },
                },
            },
        },
    };
}

MemoryAddress memory::getAddresses(const SiVersion::Full version) {
    try {
        return addressMap.at(version.first).at(version.second);
    } catch (out_of_range&) {
        throw runtime_error(format(
            "Unsupported Source Insight Version: {}{}",
            magic_enum::enum_name(version.first),
            magic_enum::enum_name(version.second)
        ));
    }
}

uint64_t memory::scanPattern(const string& pattern) {
    uint64_t varAddress = 0;
    const shared_ptr<void> sharedProcessHandle(GetCurrentProcess(), CloseHandle);
    if (sharedProcessHandle) {
        const auto BaseAddress = reinterpret_cast<uint64_t>(GetModuleHandle(nullptr));
        string scannedString;
        scannedString.resize(1024);
        SIZE_T readLength = 0;
        for (auto i = 0; i < 100000; i++) {
            if (ReadProcessMemory(
                sharedProcessHandle.get(),
                reinterpret_cast<LPCVOID>(BaseAddress + i * scannedString.size() * sizeof(char)),
                scannedString.data(),
                scannedString.size() * sizeof(char),
                &readLength
            )) {
                if (const auto found = scannedString.find(pattern); found != string::npos) {
                    varAddress = BaseAddress + i * scannedString.size() * sizeof(char) + found;
                    break;
                }
            }
        }
    }
    return varAddress;
}

optional<uint32_t> memory::readMemory32(uint64_t address, const bool relative) {
    uint32_t value = 0;
    const shared_ptr<void> sharedProcessHandle(GetCurrentProcess(), CloseHandle);
    if (sharedProcessHandle) {
        if (relative) {
            address += reinterpret_cast<uint64_t>(GetModuleHandle(nullptr));
        }
        if (ReadProcessMemory(
            sharedProcessHandle.get(),
            reinterpret_cast<LPCVOID>(address),
            &value,
            sizeof(uint32_t),
            nullptr
        )) {
            return value;
        }
    }
    return nullopt;
}

bool memory::writeMemory(const uint64_t address, const string& value) {
    const shared_ptr<void> sharedProcessHandle(GetCurrentProcess(), CloseHandle);
    if (sharedProcessHandle) {
        return WriteProcessMemory(
            sharedProcessHandle.get(),
            reinterpret_cast<LPVOID>(address),
            value.data(),
            (value.length() + 1) * sizeof(char),
            nullptr
        );
    }
    return false;
}
