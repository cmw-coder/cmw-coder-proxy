#include <format>

#include <types/CursorMonitor.h>
#include <types/RegistryMonitor.h>
#include <types/WindowInterceptor.h>
#include <utils/logger.h>
#include <utils/window.h>

using namespace std;
using namespace types;
using namespace utils;

namespace {
    const auto UM_KEYCODE = WM_USER + 0x03E9;
}

WindowInterceptor::WindowInterceptor() {
    this->_windowHook = shared_ptr<void>(SetWindowsHookEx(
            WH_CALLWNDPROC,
            WindowInterceptor::_windowProcedureHook,
            nullptr,
            GetCurrentThreadId()
    ), UnhookWindowsHookEx);
    if (!this->_windowHook) {
        throw runtime_error("Failed to set window hook.");
    }
}

void WindowInterceptor::addHandler(UserAction userAction, WindowInterceptor::CallBackFunction function) {
    _handlers[userAction] = std::move(function);
}

long WindowInterceptor::_windowProcedureHook(int nCode, unsigned int wParam, long lParam) {
    GetInstance()->_processWindowMessage(lParam);
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

void WindowInterceptor::_processWindowMessage(long lParam) {
    const auto windowProcData = reinterpret_cast<PCWPSTRUCT>(lParam);
    const auto currentWindow = windowProcData->hwnd;
    if (window::getWindowClassName(currentWindow) == "si_Sw") {
        switch (windowProcData->message) {
            case WM_KILLFOCUS: {
                if (this->_codeWindow) {
                    logger::log(format(
                            "Coding window '{}' lost focus. (0x{:08X} '{}')",
                            window::getWindowText(currentWindow),
                            reinterpret_cast<uint64_t>(currentWindow),
                            window::getWindowClassName(currentWindow)
                    ));
                    _handlers.at(UserAction::Navigate)(-1);
                    this->_codeWindow.store(nullptr);
                }
                break;
            }
            case WM_MOUSEACTIVATE: {
                CursorMonitor::GetInstance()->setAction(UserAction::Navigate);
                break;
            }
            case WM_SETFOCUS: {
                if (!this->_codeWindow) {
                    logger::log(format(
                            "Coding window '{}' gained focus. (0x{:08X} '{}')",
                            window::getWindowText(currentWindow),
                            reinterpret_cast<uint64_t>(currentWindow),
                            window::getWindowClassName(currentWindow)
                    ));
                    this->_codeWindow.store(currentWindow);
                }
                break;
            }
            case UM_KEYCODE: {
                if (this->_codeWindow.load()) {
                    _handleKeycode(windowProcData->wParam);
                }
                break;
            }
            default: {
                break;
            }
        }
    }
}

void WindowInterceptor::_handleKeycode(unsigned int keycode) noexcept {
    try {
        switch (keycode) {
            case 0x0008: { // Backspace
                CursorMonitor::GetInstance()->setAction(UserAction::DeleteBackward);
                break;
            }
            case 0x0009: { // Tab
                _handlers.at(UserAction::Accept)(keycode);
                break;
            }
            case 0x000D: { // Enter
                _handlers.at(UserAction::ModifyLine)(keycode);
                break;
            }
            case 0x001B: { // Escape
                _handlers.at(UserAction::Navigate)(keycode);
                break;
            }
            case 0x802E: { // Delete
                _handlers.at(UserAction::DeleteForward)(keycode);
                break;
            }
            case 0x045A: { // Ctrl + Z
                RegistryMonitor::GetInstance()->cancelByUndo();
                break;
            }
            default: {
                if (keycode >= 0x0020 && keycode <= 0x007E) {
                    _handlers.at(UserAction::Normal)(keycode);
                } else if (((keycode & 0x802F) >= 0x8021 && (keycode & 0x802F) <= 0x8029)) {
                    /// See "WinUser.h" Line 515
                    CursorMonitor::GetInstance()->setAction(UserAction::Navigate);
                }
                break;
            }
        }
    } catch (...) {}
}

bool WindowInterceptor::sendFunctionKey(int key) {
    return window::sendFunctionKey(reinterpret_cast<HWND>(this->_codeWindow.load()), key);
}
