#include <format>
#include <vector>

#include <types/ModuleProxy.h>
#include <utils/logger.h>
#include <utils/system.h>
#include <utils/window.h>

using namespace std;
using namespace types;
using namespace utils;

namespace {
    const auto UM_KEYCODE = WM_USER + 0x03E9;
    atomic<HWND> codeWindow = nullptr;
}

ModuleProxy::ModuleProxy(std::string &&moduleName) {
    this->_moduleName = std::move(moduleName);
}

bool ModuleProxy::load() {
    const auto modulePath = system::getSystemDirectory() + R"(\)" + _moduleName;
    this->_hModule = LoadLibrary(modulePath.data());
    this->isLoaded = this->_hModule != nullptr;
    thread([this]() {
        while (this->isLoaded.load()) {
            if (!codeWindow.load()) {
                this_thread::sleep_for(chrono::milliseconds(50));
                continue;
            }
            window::sendFunctionKey(codeWindow.load(), VK_F12);
            this_thread::sleep_for(chrono::milliseconds(1000));
        }
    }).detach();
    return this->isLoaded;
}

bool ModuleProxy::free() {
    if (!this->_hModule) {
        return false;
    }
    this->isLoaded = false;
    return FreeLibrary(this->_hModule) != FALSE;
}

RemoteFunc ModuleProxy::getRemoteFunction(const std::string &procName) {
    if (!this->_hModule) {
        return nullptr;
    }
    return reinterpret_cast<RemoteFunc>(GetProcAddress(this->_hModule, procName.c_str()));
}

bool ModuleProxy::hookWindowProc() {
    this->_windowHook = SetWindowsHookEx(
            WH_CALLWNDPROC,
            reinterpret_cast<HOOKPROC>((void *) [](INT nCode, WPARAM wParam, LPARAM lParam) -> LRESULT {
                const auto windowProcData = reinterpret_cast<PCWPSTRUCT>(lParam);
                switch (windowProcData->message) {
                    case WM_KILLFOCUS: {
                        const auto targetWindow = reinterpret_cast<HWND>(windowProcData->wParam);
                        if (window::getWindowClassName(windowProcData->hwnd) == "si_Sw" &&
                            window::getWindowClassName(targetWindow) != "si_Poplist") {
                            logger::log(format(
                                    "[WH_CALLWNDPROC] [WM_KILLFOCUS] window: '{}'(0x{:08X} '{}')",
                                    window::getWindowText(windowProcData->hwnd),
                                    reinterpret_cast<uint64_t>(windowProcData->hwnd),
                                    window::getWindowClassName(windowProcData->hwnd)
                            ));
                            if (codeWindow.load()) {
                                codeWindow = nullptr;
                            }
                        }
                        break;
                    }
                    case WM_SETFOCUS: {
                        if (window::getWindowClassName(windowProcData->hwnd) == "si_Sw") {
                            logger::log(format(
                                    "[WH_CALLWNDPROC] [WM_SETFOCUS] window: '{}'(0x{:08X} '{}')",
                                    window::getWindowText(windowProcData->hwnd),
                                    reinterpret_cast<uint64_t>(windowProcData->hwnd),
                                    window::getWindowClassName(windowProcData->hwnd)
                            ));
                            if (!codeWindow.load()) {
                                codeWindow = windowProcData->hwnd;
                            }
                        }
                        break;
                    }
                    case UM_KEYCODE: {
                        if (windowProcData->wParam != 0x1F7B) {
                            try {
                                system::setRegValue32(
                                        R"(SOFTWARE\Source Dynamics\Source Insight\3.0)",
                                        "keycode",
                                        windowProcData->wParam
                                );
                                logger::log(format(
                                        "[WH_CALLWNDPROC] [UM_KEYCODE] setRegValue32 success: 0x{:08X}, hwnd: 0x{:08X}",
                                        windowProcData->wParam,
                                        reinterpret_cast<uint64_t>(windowProcData->hwnd)
                                ));
                            } catch (runtime_error &e) {
                                logger::log(format(
                                        "[WH_CALLWNDPROC] [UM_KEYCODE] RegSetKeyValue failed: {}",
                                        e.what()
                                ));
                            }
                        }
                        break;
                    }
                    default: {
                        break;
                    }
                }
                return CallNextHookEx(nullptr, nCode, wParam, lParam);
            }),
            nullptr,
            GetCurrentThreadId()
    );

    return this->_windowHook != nullptr;
}

[[maybe_unused]] bool ModuleProxy::hookKeyboardProc() {
    this->_keyboardHook = SetWindowsHookEx(
            WH_KEYBOARD,
            reinterpret_cast<HOOKPROC>((void *) [](INT nCode, WPARAM wParam, LPARAM lParam) -> LRESULT {
                if (nCode >= 0) {
                    WORD vkCode = LOWORD(wParam);
                    WORD keyFlags = HIWORD(lParam);
                    WORD repeatCount = LOWORD(lParam);  // > 0 if several keydown messages was combined into one message
                    WORD scanCode = LOBYTE(keyFlags);

                    // extended-key flag, 1 if scancode has 0xE0 prefix
                    if ((keyFlags & KF_EXTENDED) == KF_EXTENDED) {
                        scanCode = MAKEWORD(scanCode, 0xE0);
                    }
                    bool wasKeyDown = (keyFlags & KF_REPEAT) == KF_REPEAT;  // previous key-state flag, 1 on auto-repeat
                    bool isKeyReleased = (keyFlags & KF_UP) == KF_UP;   // transition-state flag, 1 on keyup

                    logger::log(format(
                            "[WH_KEYBOARD] vkCode: 0x{:04X}, scanCode: 0x{:04X}, wasKeyDown: {}, repeatCount: {}, isKeyReleased: {}",
                            vkCode,
                            scanCode,
                            wasKeyDown,
                            repeatCount,
                            isKeyReleased
                    ));
                }
                return CallNextHookEx(nullptr, nCode, wParam, lParam);
            }),
            nullptr,
            GetCurrentThreadId()
    );

    return this->_keyboardHook != nullptr;
}

bool ModuleProxy::unhookWindowProc() {
    return UnhookWindowsHookEx(this->_windowHook);
}

[[maybe_unused]] bool ModuleProxy::unhookKeyboardProc() {
    return UnhookWindowsHookEx(this->_keyboardHook);
}
