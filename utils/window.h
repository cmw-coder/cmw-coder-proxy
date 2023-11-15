#pragma once

#include <string>

#include <types/common.h>
#include <types/Key.h>

namespace utils::window {
    std::string getWindowClassName(int64_t hwnd);

    std::string getWindowText(int64_t hwnd);

    bool postKeycode(int64_t hwnd, types::Keycode keycode);

    bool sendKeycode(int64_t hwnd, types::Keycode keycode);
}