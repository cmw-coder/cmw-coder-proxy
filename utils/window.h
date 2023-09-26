#pragma once

#include <string>

#include <types/SiVersion.h>

#include <Windows.h>

namespace utils::window {
    std::string getWindowClassName(HWND hwnd);

    std::string getWindowText(HWND hwnd);

    bool sendFunctionKey(HWND hwnd, int key);
}