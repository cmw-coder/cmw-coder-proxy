#include <magic_enum.hpp>

#include <types/common.h>
#include <utils/window.h>

#include <Windows.h>

using namespace std;
using namespace types;
using namespace utils;

string window::getWindowClassName(const int64_t hwnd) {
    string text;
    text.resize(256);
    return text.substr(0, GetClassName(reinterpret_cast<HWND>(hwnd), text.data(), 256));
}

tuple<int64_t, int64_t> window::getClientPosition(const int64_t hwnd) {
    POINT ptClient = {};
    ClientToScreen(reinterpret_cast<HWND>(hwnd), &ptClient);

    return {ptClient.x, ptClient.y};
}

tuple<int64_t, int64_t> window::getWindowPosition(const int64_t hwnd) {
    RECT rect;
    GetWindowRect(reinterpret_cast<HWND>(hwnd), &rect);
    return {rect.left, rect.top};
}

string window::getWindowText(const int64_t hwnd) {
    string text;
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
