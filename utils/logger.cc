#include <format>

#include <utils/logger.h>

#include <windows.h>

using namespace std;
using namespace utils;

namespace {
    std::string logDistinguish = "cmw-coder-proxy";
}

void logger::debug(const std::string& message) {
    OutputDebugString(format("[{}][debug] {}\n", logDistinguish, message).c_str());
}

void logger::error(const string& message) {
    MessageBox(nullptr, message.c_str(), "Error", MB_OK | MB_ICONERROR);
}

void logger::info(const std::string& message) {
    OutputDebugString(format("[{}][info] {}\n", logDistinguish, message).c_str());
}

void logger::log(const std::string& message) {
    OutputDebugString(format("[{}][log] {}\n", logDistinguish, message).c_str());
}

void logger::warn(const std::string& message) {
    OutputDebugString(format("[{}][warn] {}\n", logDistinguish, message).c_str());
}
