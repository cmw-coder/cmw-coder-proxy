#pragma once

#include <string>

#include <types/Key.h>

namespace utils::window {
    std::string getWindowClassName(void *hwnd);

    std::string getWindowText(void *hwnd);

    bool sendKeycode(void *hwnd, int keycode);
}