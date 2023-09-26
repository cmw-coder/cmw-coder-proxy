#include <format>

#include <utils/logger.h>

#include <windows.h>

using namespace std;
using namespace utils;

namespace {
    std::string logDistinguish = "si-coding-hook";
}

void logger::log(const std::string &message) {
    OutputDebugString(format("[{}] {}", logDistinguish, message).c_str());
}