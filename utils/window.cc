#include <magic_enum.hpp>

#include <types/common.h>
#include <utils/window.h>

#include <Windows.h>

using namespace std;
using namespace types;
using namespace utils;

namespace {}

std::string window::getWindowClassName(void *hwnd) {
    std::string text;
    text.resize(256);
    return text.substr(0, GetClassName(reinterpret_cast<HWND>(hwnd), text.data(), 256));
}

std::string window::getWindowText(void *hwnd) {
    std::string text;
    text.resize(256);
    return text.substr(0, GetWindowText(reinterpret_cast<HWND>(hwnd), text.data(), 256));
}

bool window::sendKeycode(int64_t hwnd, int keycode) {
//    if (siVersion == SiVersion::New) {
//        offset = offset << 8;
//    }
    if (hwnd < 0) {
        return false;
    }
    return PostMessage((HWND__ *) hwnd, UM_KEYCODE, keycode, 0) != 0;
}