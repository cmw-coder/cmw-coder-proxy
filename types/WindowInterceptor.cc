#include <format>

#include <magic_enum.hpp>

#include <helpers/KeyHelper.h>
#include <types/common.h>
#include <types/CursorMonitor.h>
#include <types/RegistryMonitor.h>
#include <types/WindowInterceptor.h>
#include <utils/logger.h>
#include <utils/window.h>

#include <windows.h>

using namespace helpers;
using namespace magic_enum;
using namespace std;
using namespace types;
using namespace utils;

namespace {}

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
//                    _handlers.at(UserAction::Navigate)(-1);
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
            case enum_integer(Key::BackSpace): {
                CursorMonitor::GetInstance()->setAction(UserAction::DeleteBackward);
                break;
            }
            case enum_integer(Key::Tab): {
                _handlers.at(UserAction::Accept)(keycode);
                break;
            }
            case enum_integer(Key::Enter): {
                _handlers.at(UserAction::ModifyLine)(keycode);
                break;
            }
            case enum_integer(Key::Escape): {
                _handlers.at(UserAction::Navigate)(keycode);
                break;
            }
            case enum_integer(Key::Delete): {
                _handlers.at(UserAction::DeleteForward)(keycode);
                break;
            }
            case enum_integer(Modifier::Ctrl) + enum_integer(Key::S): {
                RegistryMonitor::GetInstance()->cancelBySave();
                break;
            }
            case enum_integer(Modifier::Ctrl) + enum_integer(Key::Z): {
                RegistryMonitor::GetInstance()->cancelByUndo();
                break;
            }
            default: {
                if (keycode >= enum_integer(Key::Space) && keycode <= enum_integer(Key::Tilde) &&
                    keycode != enum_integer(Key::RightCurlyBracket)) {
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

bool WindowInterceptor::sendAcceptCompletion() {
    return window::sendKeycode(
            this->_codeWindow.load(),
            KeyHelper::toKeycode(Key::F10, {Modifier::Shift, Modifier::Ctrl, Modifier::Alt})
    );
}

bool WindowInterceptor::sendCancelCompletion() {
    return window::sendKeycode(
            this->_codeWindow.load(),
            KeyHelper::toKeycode(Key::F9, {Modifier::Shift, Modifier::Ctrl, Modifier::Alt})
    );
}

bool WindowInterceptor::sendInsertCompletion() {
    return window::sendKeycode(
            this->_codeWindow.load(),
            KeyHelper::toKeycode(Key::F12, {Modifier::Shift, Modifier::Ctrl, Modifier::Alt})
    );
}

bool WindowInterceptor::sendRetrieveInfo() {
    logger::log("Retrieving editor info...");
    return window::sendKeycode(
            this->_codeWindow.load(),
            KeyHelper::toKeycode(Key::F11, {Modifier::Shift, Modifier::Ctrl, Modifier::Alt})
    );
}

bool WindowInterceptor::sendSave() {
    return window::sendKeycode(
            this->_codeWindow.load(),
            KeyHelper::toKeycode(Key::S, Modifier::Ctrl)
    );
}

bool WindowInterceptor::sendUndo() {
    return window::sendKeycode(
            this->_codeWindow.load(),
            KeyHelper::toKeycode(Key::Z, Modifier::Ctrl)
    );
}