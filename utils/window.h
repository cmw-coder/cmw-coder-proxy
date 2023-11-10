#pragma once

#include <string>

#include <types/common.h>
#include <types/Key.h>

namespace utils::window {
    std::string getWindowClassName(void *hwnd);

    std::string getWindowText(void *hwnd);

    bool postKeycode(int64_t hwnd, types::Keycode keycode);

    bool sendKeycode(int64_t hwnd, types::Keycode keycode);
}