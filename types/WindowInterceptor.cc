#include <format>

#include <types/WindowInterceptor.h>
#include <utils/logger.h>
#include <utils/system.h>
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
    thread([this]() {
        bool needDelay = true;
        while (this->_windowHook) {
            if (!this->_codeWindow.load()) {
                needDelay = true;
                this_thread::sleep_for(chrono::milliseconds(50));
                continue;
            }
            if (needDelay) {
                needDelay = false;
                this_thread::sleep_for(chrono::milliseconds(2000));
                continue;
            }
            window::sendFunctionKey(reinterpret_cast<HWND>(this->_codeWindow.load()), _siVersion, VK_F12);
            this_thread::sleep_for(chrono::milliseconds(50));
        }
    }).detach();
    const auto currentModuleName = system::getModuleFileName(reinterpret_cast<uint64_t>(GetModuleHandle(nullptr)));
    if (currentModuleName.find("Insight3.exe") != string::npos) {
        _siVersion.store(SiVersion::Old);
    } else if (currentModuleName.find("sourceinsight4.exe") != string::npos) {
        _siVersion.store(SiVersion::New);
    }
}

void WindowInterceptor::addHandler(ActionType keyType, WindowInterceptor::CallBackFunction function) {
    _handlers[keyType] = std::move(function);
}

long WindowInterceptor::_windowProcedureHook(int nCode, unsigned int wParam, long lParam) {
    GetInstance()->_processWindowMessage(lParam);
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

void WindowInterceptor::_processWindowMessage(long lParam) {
    const auto windowProcData = reinterpret_cast<PCWPSTRUCT>(lParam);
    switch (windowProcData->message) {
        case WM_KILLFOCUS: {
            const auto currentWindow = windowProcData->hwnd;
            if (window::getWindowClassName(currentWindow) == "si_Sw" && this->_codeWindow) {
                logger::log(format(
                        "Coding window '{}' lost focus. (0x{:08X} '{}')",
                        window::getWindowText(currentWindow),
                        reinterpret_cast<uint64_t>(currentWindow),
                        window::getWindowClassName(currentWindow)
                ));
                this->_codeWindow.store(nullptr);
            }
            break;
        }
        case WM_MBUTTONUP: {
            if (_cursorMonitor.checkPosition()) {
                _handlers.at(ActionType::Navigate)(-1);
            }
            break;
        }
        case WM_SETFOCUS: {
            const auto currentWindow = windowProcData->hwnd;
            if (window::getWindowClassName(currentWindow) == "si_Sw" && !this->_codeWindow) {
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
//            if (keycode != 0x1F7B) {
            _handleKeycode(windowProcData->wParam);
            break;
        }
        default: {
            break;
        }
    }
}

void WindowInterceptor::_handleKeycode(unsigned int keycode) noexcept {
    try {
        switch (keycode) {
            case 0x0008: { // Backspace
                const auto positions = _cursorMonitor.checkPosition();
                if (positions.has_value()) {
                    const auto [oldPosition, newPosition] = positions.value();
                    if (oldPosition.line == newPosition.line) {
                        _handlers.at(ActionType::DeleteBackward)(keycode);
                    } else {
                        _handlers.at(ActionType::ModifyLine)(keycode);
                    }
                }
                break;
            }
            case 0x0009: { // Tab
                _handlers.at(ActionType::Accept)(keycode);
                break;
            }
            case 0x000D: { // Enter
                _handlers.at(ActionType::ModifyLine)(keycode);
                break;
            }
            case 0x001B: { // Escape
                _handlers.at(ActionType::Navigate)(keycode);
                break;
            }
            case 0x802E: { // Delete
                _handlers.at(ActionType::DeleteForward)(keycode);
                break;
            }
            default: {
                if (keycode >= 0x0020 && keycode <= 0x007E) {
                    _handlers.at(ActionType::Normal)(keycode);
                } else if (((keycode & 0x802F) >= 0x8021 && (keycode & 0x802F) <= 0x8029)) {
                    /// See "WinUser.h" Line 515
                    if (_cursorMonitor.checkPosition()) {
                        _handlers.at(ActionType::Navigate)(keycode);
                    }
                }
                break;
            }
        }
    } catch (...) {}
}

bool WindowInterceptor::sendFunctionKey(int key) {
    return window::sendFunctionKey(reinterpret_cast<HWND>(this->_codeWindow.load()), _siVersion, key);
}
