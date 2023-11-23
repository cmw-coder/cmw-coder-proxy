#include <magic_enum.hpp>

#include <types/common.h>
#include <components/Configurator.h>
#include <types/CursorMonitor.h>
#include <types/RegistryMonitor.h>
#include <types/WindowInterceptor.h>
#include <utils/logger.h>
#include <utils/window.h>

#include <windows.h>

using namespace components;
using namespace helpers;
using namespace magic_enum;
using namespace std;
using namespace types;
using namespace utils;

WindowInterceptor::WindowInterceptor() : _keyHelper(Configurator::GetInstance()->version().first),
                                         _windowHook(shared_ptr<void>(SetWindowsHookEx(
                                                                          WH_CALLWNDPROC,
                                                                          _windowProcedureHook,
                                                                          nullptr,
                                                                          GetCurrentThreadId()
                                                                      ), UnhookWindowsHookEx)) {
    if (!_windowHook) {
        throw runtime_error("Failed to set window hook.");
    }

    _threadDebounceRetrieveInfo();
}

[[maybe_unused]] void
WindowInterceptor::addHandler(const UserAction userAction, CallBackFunction function) {
    _handlers[userAction] = std::move(function);
}

void WindowInterceptor::requestRetrieveInfo() {
    _debounceTime.store(chrono::high_resolution_clock::now() + chrono::milliseconds(250));
    _needRetrieveInfo.store(true);
}

bool WindowInterceptor::sendAcceptCompletion() {
    _cancelRetrieveInfo();
    return window::postKeycode(
        _codeWindow,
        _keyHelper.toKeycode(Key::F10, {Modifier::Shift, Modifier::Ctrl, Modifier::Alt})
    );
}

bool WindowInterceptor::sendCancelCompletion() {
    _cancelRetrieveInfo();
    return window::postKeycode(
        _codeWindow,
        _keyHelper.toKeycode(Key::F9, {Modifier::Shift, Modifier::Ctrl, Modifier::Alt})
    );
}

bool WindowInterceptor::sendInsertCompletion() {
    _cancelRetrieveInfo();
    return window::postKeycode(
        _codeWindow,
        _keyHelper.toKeycode(Key::F12, {Modifier::Shift, Modifier::Ctrl, Modifier::Alt})
    );
}

bool WindowInterceptor::sendSave() {
    _cancelRetrieveInfo();
    return window::postKeycode(
        _codeWindow,
        _keyHelper.toKeycode(Key::S, Modifier::Ctrl)
    );
}

bool WindowInterceptor::sendUndo() {
    _cancelRetrieveInfo();
    return window::postKeycode(
        _codeWindow,
        _keyHelper.toKeycode(Key::Z, Modifier::Ctrl)
    );
}

void WindowInterceptor::_cancelRetrieveInfo() {
    _needRetrieveInfo.store(false);
}

void WindowInterceptor::_handleKeycode(Keycode keycode) noexcept {
    if (_keyHelper.isNavigate(keycode)) {
        CursorMonitor::GetInstance()->setAction(UserAction::Navigate);
        return;
    }

    if (_keyHelper.isPrintable(keycode)) {
        window::sendKeycode(_codeWindow, _keyHelper.toKeycode(Key::Insert));
        window::sendKeycode(_codeWindow, _keyHelper.toKeycode(Key::Insert));
        _handlers.at(UserAction::Normal)(keycode);
        return;
    }

    const auto keyCombinationOpt = _keyHelper.fromKeycode(keycode);
    try {
        if (keyCombinationOpt.has_value()) {
            if (const auto [key, modifiers] = keyCombinationOpt.value(); modifiers.empty()) {
                switch (key) {
                    case Key::BackSpace: {
                        window::sendKeycode(_codeWindow, _keyHelper.toKeycode(Key::Insert));
                        window::sendKeycode(_codeWindow, _keyHelper.toKeycode(Key::Insert));
                        CursorMonitor::GetInstance()->setAction(UserAction::DeleteBackward);
                        break;
                    }
                    case Key::Tab: {
                        _handlers.at(UserAction::Accept)(keycode);
                        break;
                    }
                    case Key::Enter: {
                        _handlers.at(UserAction::ModifyLine)(keycode);
                        break;
                    }
                    case Key::Escape: {
                        if (Configurator::GetInstance()->version().first == SiVersion::Major::V40) {
                            thread([this, keycode] {
                                try {
                                    this_thread::sleep_for(chrono::milliseconds(150));
                                    _handlers.at(UserAction::Navigate)(keycode);
                                }
                                catch (...) {
                                }
                            }).detach();
                        }
                        else {
                            _handlers.at(UserAction::Navigate)(keycode);
                        }
                        break;
                    }
                    case Key::Delete: {
                        _handlers.at(UserAction::DeleteForward)(keycode);
                        break;
                    }
                    default: {
                        break;
                    }
                }
            }
            else {
                if (modifiers.size() == 1 && modifiers.contains(Modifier::Ctrl)) {
                    switch (key) {
                        case Key::S: {
                            RegistryMonitor::GetInstance()->cancelBySave();
                            break;
                        }
                        case Key::V: {
                            _cancelRetrieveInfo();
                            break;
                        }
                        case Key::Z: {
                            RegistryMonitor::GetInstance()->cancelByUndo();
                            break;
                        }
                        default: {
                            break;
                        }
                    }
                }
            }
        }
    }
    catch (...) {
    }
}

void WindowInterceptor::_processWindowMessage(const long lParam) {
    const auto windowProcData = reinterpret_cast<PCWPSTRUCT>(lParam);
    const auto currentWindow = reinterpret_cast<int64_t>(windowProcData->hwnd);
    if (const auto currentWindowClass = window::getWindowClassName(currentWindow); currentWindowClass == "si_Sw") {
        switch (windowProcData->message) {
            case WM_KILLFOCUS: {
                if (const auto targetWindowClass = window::getWindowClassName(windowProcData->wParam);
                    _codeWindow >= 0 && targetWindowClass != "si_Poplist") {
                    // logger::log(format(
                    //         "Coding window '{}' lost focus. (0x{:08X} '{}') to (0x{:08X} '{}')",
                    //         window::getWindowText(currentWindow),
                    //         currentWindow,
                    //         currentWindowClass,
                    //         static_cast<uint64_t>(windowProcData->wParam),
                    //         targetWindowClass
                    // ));
                    _handlers.at(UserAction::Navigate)(-1);
                    _codeWindow.store(-1);
                }
                else if (targetWindowClass == "si_Poplist") {
                    _popListWindow.store(windowProcData->wParam);
                }
                break;
            }
            case WM_MOUSEACTIVATE: {
                CursorMonitor::GetInstance()->setAction(UserAction::Navigate);
                break;
            }
            case WM_SETFOCUS: {
                // const auto targetWindowClass = window::getWindowClassName(windowProcData->wParam);
                // logger::log(format(
                //         "Coding window '{}' set focus. (0x{:08X} '{}') from (0x{:08X} '{}')",
                //         window::getWindowText(currentWindow),
                //         currentWindow,
                //         currentWindowClass,
                //         static_cast<uint64_t>(windowProcData->wParam),
                //         targetWindowClass
                // ));
                if (_codeWindow < 0) {
                    _codeWindow.store(currentWindow);
                }
                if (_popListWindow > 0) {
                    _popListWindow.store(-1);
                    sendCancelCompletion();
                }
                break;
            }
            case UM_KEYCODE: {
                _handleKeycode(windowProcData->wParam);
                break;
            }
            default: {
                break;
            }
        }
    }
}

void WindowInterceptor::_threadDebounceRetrieveInfo() {
    thread([this] {
        while (_isRunning.load()) {
            if (_needRetrieveInfo.load()) {
                if (const auto deltaTime = _debounceTime.load() - chrono::high_resolution_clock::now();
                    deltaTime <= chrono::nanoseconds(0)) {
                    logger::log("Sending retrieve info...");
                    window::postKeycode(
                        _codeWindow,
                        _keyHelper.toKeycode(Key::F11, {Modifier::Shift, Modifier::Ctrl, Modifier::Alt})
                    );
                    _needRetrieveInfo.store(false);
                }
                else {
                    this_thread::sleep_for(deltaTime);
                }
            }
            else {
                this_thread::sleep_for(chrono::milliseconds(10));
            }
        }
    }).detach();
}

long WindowInterceptor::_windowProcedureHook(const int nCode, const unsigned int wParam, const long lParam) {
    GetInstance()->_processWindowMessage(lParam);
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}
