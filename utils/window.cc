#include <magic_enum.hpp>

#include <types/common.h>
#include <utils/common.h>
#include <utils/window.h>

#include <Windows.h>

using namespace std;
using namespace types;
using namespace utils;

uint32_t window::getMainWindowHandle(const uint32_t processId) {
    struct Payload {
        const uint32_t targetProcessId;
        HWND resultWindowHandle;
    } payload{.targetProcessId = processId};
    EnumWindows([](HWND__* currentHandle, const LPARAM lParam) {
        const auto payloadPtr = reinterpret_cast<Payload *>(lParam);
        DWORD currentProcessId = 0;
        GetWindowThreadProcessId(currentHandle, &currentProcessId);
        if (payloadPtr->targetProcessId == currentProcessId &&
            GetWindow(currentHandle, GW_OWNER) == nullptr &&
            IsWindowVisible(currentHandle)) {
            payloadPtr->resultWindowHandle = currentHandle;
            return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&payload));
    return reinterpret_cast<uint32_t>(payload.resultWindowHandle);
}

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

ModifierSet window::getModifierKeys() {
    ModifierSet result;
    if (!common::checkKeyIsUp(VK_MENU)) {
        result.insert(Modifier::Alt);
    }
    if (!common::checkKeyIsUp(VK_CONTROL)) {
        result.insert(Modifier::Ctrl);
    }
    if (!common::checkKeyIsUp(VK_SHIFT)) {
        result.insert(Modifier::Shift);
    }
    return result;
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
