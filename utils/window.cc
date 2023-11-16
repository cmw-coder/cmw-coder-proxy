#include <magic_enum.hpp>

#include <types/common.h>
#include <utils/window.h>

#include <Windows.h>

using namespace std;
using namespace types;
using namespace utils;

std::string window::getWindowClassName(const int64_t hwnd) {
    std::string text;
    text.resize(256);
    return text.substr(0, GetClassName(reinterpret_cast<HWND>(hwnd), text.data(), 256));
}

std::string window::getWindowText(const int64_t hwnd) {
    std::string text;
    text.resize(256);
    return text.substr(0, GetWindowText(reinterpret_cast<HWND>(hwnd), text.data(), 256));
}

bool window::postKeycode(const int64_t hwnd, const Keycode keycode) {
    if (hwnd < 0) {
        return false;
    }
    return PostMessage(reinterpret_cast<HWND>(hwnd), UM_KEYCODE, keycode, 0) != 0;
}

bool window::sendKeycode(const int64_t hwnd, const Keycode keycode) {
    if (hwnd < 0) {
        return false;
    }
    return SendMessage(reinterpret_cast<HWND>(hwnd), UM_KEYCODE, keycode, 0) != 0;
}
