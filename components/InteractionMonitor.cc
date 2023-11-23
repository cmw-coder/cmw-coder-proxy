#include <format>

#include <components/Configurator.h>
#include <components/InteractionMonitor.h>
#include <types/SiVersion.h>
#include <utils/window.h>

#include <windows.h>

#include "WindowManager.h"

using namespace components;
using namespace std;
using namespace types;
using namespace utils;

namespace {
    const unordered_map<SiVersion::Major, unordered_map<SiVersion::Minor, tuple<uint64_t, uint64_t>>> addressMap = {
        {
            SiVersion::Major::V35, {
                {SiVersion::Minor::V0076, {0x1CBEFC, 0x1CBF00}},
                {SiVersion::Minor::V0086, {0x1BE0CC, 0x1CD3E0}}
            }
        },
        {
            SiVersion::Major::V40, {
                {SiVersion::Minor::V0084, {0x268A60, 0x268A64}},
                {SiVersion::Minor::V0086, {0x26D938, 0x26D93C}},
                {SiVersion::Minor::V0088, {0x26EA08, 0x26EA0C}},
                {SiVersion::Minor::V0089, {0x26EAC8, 0x26EACC}},
                {SiVersion::Minor::V0096, {0x278D30, 0x278D34}},
                {SiVersion::Minor::V0116, {0x27E468, 0x27E46C}},
                {SiVersion::Minor::V0118, {0x27F490, 0x27F494}},
                {SiVersion::Minor::V0120, {0x2807F8, 0x2807FC}},
                {SiVersion::Minor::V0124, {0x284DF0, 0x284DF4}},
                {SiVersion::Minor::V0130, {0x289F9C, 0x289FA0}},
                {SiVersion::Minor::V0132, {0x28B2FC, 0x28B300}}
            }
        },
    };
}

InteractionMonitor::InteractionMonitor() : _keyHelper(Configurator::GetInstance()->version().first),
                                           _processHandle(GetCurrentProcess(), CloseHandle),
                                           _windowHookHandle(
                                               SetWindowsHookEx(
                                                   WH_CALLWNDPROC,
                                                   _windowProcedureHook,
                                                   nullptr,
                                                   GetCurrentThreadId()
                                               ),
                                               UnhookWindowsHookEx
                                           ) {
    if (!_processHandle) {
        throw runtime_error("Failed to get Source Insight's process handle");
    }
    if (!_windowHookHandle) {
        throw runtime_error("Failed to set window hook.");
    }
    const auto baseAddress = reinterpret_cast<uint32_t>(GetModuleHandle(nullptr));
    const auto [majorVersion, minorVersion] = Configurator::GetInstance()->version();
    try {
        const auto [lineOffset, charOffset] = addressMap.at(majorVersion).at(minorVersion);
        _cursorLineAddress = baseAddress + lineOffset;
        _cursorCharAddress = baseAddress + charOffset;
    }
    catch (out_of_range&e) {
        throw runtime_error(format("Unsupported Source Insight Version: ", e.what()));
    }
}

CursorPosition InteractionMonitor::_getCursorPosition() const {
    CursorPosition cursorPosition{};
    ReadProcessMemory(
        _processHandle.get(),
        reinterpret_cast<LPCVOID>(_cursorLineAddress),
        &cursorPosition.line,
        sizeof(cursorPosition.line),
        nullptr
    );
    ReadProcessMemory(
        _processHandle.get(),
        reinterpret_cast<LPCVOID>(_cursorCharAddress),
        &cursorPosition.character,
        sizeof(cursorPosition.character),
        nullptr
    );
    return cursorPosition;
}

void InteractionMonitor::_handleKeycode(Keycode keycode) const noexcept {
    const auto currentCursorPosition = _getCursorPosition();
    if (_keyHelper.isNavigate(keycode)) {
        _handlers.at(Interaction::Navigate)({});
        return;
    }

    if (_keyHelper.isPrintable(keycode)) {
        (void)WindowManager::GetInstance()->sendDoubleInsert();
        _handlers.at(Interaction::NormalInput)(make_pair(currentCursorPosition, keycode));
        return;
    }

    const auto keyCombinationOpt = _keyHelper.fromKeycode(keycode);
    try {
        if (keyCombinationOpt.has_value()) {
            if (const auto [key, modifiers] = keyCombinationOpt.value(); modifiers.empty()) {
                switch (key) {
                    case Key::BackSpace: {
                        (void)WindowManager::GetInstance()->sendDoubleInsert();
                        _handlers.at(Interaction::DeleteInput)(currentCursorPosition);
                        break;
                    }
                    case Key::Tab: {
                        _handlers.at(Interaction::AcceptCompletion)({});
                        break;
                    }
                    case Key::Enter: {
                        _handlers.at(Interaction::EnterInput)(currentCursorPosition);
                        break;
                    }
                    case Key::Escape: {
                        if (Configurator::GetInstance()->version().first == SiVersion::Major::V40) {
                            thread([this, currentCursorPosition] {
                                try {
                                    this_thread::sleep_for(chrono::milliseconds(150));
                                    _handlers.at(Interaction::Navigate)(currentCursorPosition);
                                }
                                catch (...) {
                                }
                            }).detach();
                        }
                        else {
                            _handlers.at(Interaction::Navigate)(currentCursorPosition);
                        }
                        break;
                    }
                    default: {
                        // TODO: Support Key::Delete
                        break;
                    }
                }
            }
            else {
                if (modifiers.size() == 1 && modifiers.contains(Modifier::Ctrl)) {
                    switch (key) {
                        case Key::S: {
                            _handlers.at(Interaction::Save)({});
                            break;
                        }
                        case Key::V: {
                            _handlers.at(Interaction::Paste)({});
                            break;
                        }
                        case Key::Z: {
                            _handlers.at(Interaction::Undo)({});
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

void InteractionMonitor::_processWindowMessage(const long lParam) const {
    const auto windowProcData = reinterpret_cast<PCWPSTRUCT>(lParam);
    if (const auto currentWindow = reinterpret_cast<int64_t>(windowProcData->hwnd);
        window::getWindowClassName(currentWindow) == "si_Sw") {
        switch (windowProcData->message) {
            case WM_KILLFOCUS: {
                if (WindowManager::GetInstance()->checkLostFocus(windowProcData->wParam)) {
                    _handlers.at(Interaction::LostFocus)(windowProcData->wParam);
                }
                break;
            }
            case WM_MOUSEACTIVATE: {
                _handlers.at(Interaction::MouseClick)(_getCursorPosition());
                break;
            }
            case WM_SETFOCUS: {
                if (WindowManager::GetInstance()->checkGainFocus(currentWindow)) {
                    _handlers.at(Interaction::CancelCompletion)({});
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

void InteractionMonitor::_threadMonitorCursorPosition() {
    thread([this] {
        while (_isRunning.load()) {
            this->_cursorPosition.store(_getCursorPosition());
            this_thread::sleep_for(chrono::milliseconds(1));
        }
    }).detach();
}

long InteractionMonitor::_windowProcedureHook(const int nCode, const unsigned int wParam, const long lParam) {
    GetInstance()->_processWindowMessage(lParam);
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}
