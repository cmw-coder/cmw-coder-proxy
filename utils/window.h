#pragma once

#include <string>

#include <types/common.h>
#include <types/keys.h>

namespace utils::window {
    uint32_t getMainWindowHandle(uint32_t processId);

    std::string getWindowClassName(int64_t hwnd);

    std::tuple<int64_t, int64_t> getClientPosition(int64_t hwnd);

    types::ModifierSet getModifierKeys(uint8_t currentKeycode);

    std::tuple<int64_t, int64_t> getWindowPosition(int64_t hwnd);

    std::string getWindowText(int64_t hwnd);

    bool postKeycode(int64_t hwnd, types::Keycode keycode);

    bool sendKeycode(int64_t hwnd, types::Keycode keycode);

    bool sendKeyInput(uint16_t keycode, const types::ModifierSet& modifiers = {});
}
