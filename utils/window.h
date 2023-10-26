#pragma once

#include <string>

#include <types/Key.h>

namespace utils::window {
    std::string getWindowClassName(void *hwnd);

    std::string getWindowText(void *hwnd);

    bool postKeycode(int64_t hwnd, int keycode);

    bool sendKeycode(int64_t hwnd, int keycode);
}