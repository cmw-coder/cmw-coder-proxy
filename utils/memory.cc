#include <format>
#include <memory>

#include <utils/memory.h>

#include <windows.h>

using namespace std;
using namespace utils;

namespace {
    const auto baseAddress = reinterpret_cast<uint32_t>(GetModuleHandle(nullptr));
}

uint32_t memory::offset(const uint32_t offset) {
    return baseAddress + offset;
}
