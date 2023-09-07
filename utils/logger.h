#pragma once

#include <format>
#include <string>

namespace {
    std::string moduleName = "si-coding-hook";
}

namespace utils::logger {
    void log(const std::string &message) {
        OutputDebugString(format("[{}] {}", moduleName, message).c_str());
    }
}