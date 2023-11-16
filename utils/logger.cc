#include <format>

#include <utils/logger.h>

#include <windows.h>

using namespace std;
using namespace utils;

namespace {
    std::string logDistinguish = "cmw-coder-proxy";
}

void logger::log(const std::string&message) {
    OutputDebugString(format("[{}] {}\n", logDistinguish, message).c_str());
}

void logger::error(const string&message) {
    MessageBox(nullptr, message.c_str(), "Error", MB_OK | MB_ICONERROR);
}
