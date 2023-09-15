#pragma once

#include <string>

#include <Windows.h>

namespace utils::window {
    enum class SiVersion {
        Unknown = 0,
        Old = 3,
        New = 4,
    };

    std::string getWindowClassName(HWND hwnd);

    std::string getWindowText(HWND hwnd);

    bool sendFunctionKey(HWND hwnd, SiVersion siVersion, int key);
}