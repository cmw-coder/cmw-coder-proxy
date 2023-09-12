#include <utils/window.h>

using namespace std;
using namespace utils;

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
