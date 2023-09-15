#include <utils/window.h>

using namespace std;
using namespace utils;

namespace {
    const auto UM_KEYCODE = WM_USER + 0x03E9;
}

std::string window::getWindowClassName(HWND hwnd) {
    std::string text;
    text.resize(256);
    return text.substr(0, GetClassName(hwnd, text.data(), 256));
}

std::string window::getWindowText(HWND hwnd) {
    std::string text;
    text.resize(256);
    return text.substr(0, GetWindowText(hwnd, text.data(), 256));
}

bool window::sendFunctionKey(HWND hwnd, int key) {
    int offset = 0x1000;
    // alt
    offset += 0x800;
    // ctrl
    offset += 0x400;
    // shift
    offset += 0x300;
//    if (siVersion == SiVersion::New) {
//        offset = offset << 8;
//    }
    return PostMessage(hwnd, UM_KEYCODE, offset + key, 0) != 0;
}
