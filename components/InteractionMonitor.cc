#include <format>
#include <regex>

#include <magic_enum.hpp>

#include <components/ConfigManager.h>
#include <components/InteractionMonitor.h>
#include <components/MemoryManipulator.h>
#include <components/WebsocketManager.h>
#include <components/WindowManager.h>
#include <utils/logger.h>
#include <utils/system.h>
#include <utils/window.h>

#include <windows.h>

using namespace components;
using namespace magic_enum;
using namespace models;
using namespace std;
using namespace types;
using namespace utils;

namespace {
    const auto mainThreadId = system::getMainThreadId();
}

InteractionMonitor::InteractionMonitor()
    : _keyHelper(ConfigManager::GetInstance()->version().first),
      _cbtHookHandle(
          SetWindowsHookEx(
              WH_CBT,
              _cbtProcedureHook,
              GetModuleHandle(nullptr),
              mainThreadId
          ),
          UnhookWindowsHookEx
      ),
      _keyHookHandle(
          SetWindowsHookEx(
              WH_KEYBOARD,
              _keyProcedureHook,
              GetModuleHandle(nullptr),
              mainThreadId
          ),
          UnhookWindowsHookEx
      ),
      _mouseHookHandle(
          SetWindowsHookEx(
              WH_MOUSE,
              _mouseProcedureHook,
              GetModuleHandle(nullptr),
              mainThreadId
          ),
          UnhookWindowsHookEx
      ),
      _processHandle(GetCurrentProcess(), CloseHandle),
      _windowHookHandle(
          SetWindowsHookEx(
              WH_CALLWNDPROC,
              _windowProcedureHook,
              GetModuleHandle(nullptr),
              mainThreadId
          ),
          UnhookWindowsHookEx
      ) {
    if (!_processHandle) {
        logger::error("Failed to get Source Insight's process handle");
        abort();
    }
    if (!_windowHookHandle) {
        logger::error("Failed to set window hook.");
        abort();
    }

    _threadMonitorCaretPosition();
    _threadReleaseInteractionLock();

    logger::info("InteractionMonitor is initialized.");
}

InteractionMonitor::~InteractionMonitor() {
    _isRunning.store(false);
    _interactionMutex.unlock();
}

shared_lock<shared_mutex> InteractionMonitor::getInteractionLock() const {
    return shared_lock(_interactionMutex);
}

long InteractionMonitor::_cbtProcedureHook(const int nCode, const unsigned int wParam, const long lParam) {
    if (nCode == HCBT_DESTROYWND) {
        WindowManager::GetInstance()->closeWindowHandle(wParam);
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

long InteractionMonitor::_keyProcedureHook(const int nCode, const unsigned int wParam, const long lParam) {
    if (GetInstance()->_processKeyMessage(wParam, lParam)) {
        return true;
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

long InteractionMonitor::_mouseProcedureHook(const int nCode, const unsigned int wParam, const long lParam) {
    GetInstance()->_processMouseMessage(wParam);
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

long InteractionMonitor::_windowProcedureHook(const int nCode, const unsigned int wParam, const long lParam) {
    GetInstance()->_processWindowMessage(lParam);
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

void InteractionMonitor::_handleKeycode(const Keycode keycode) noexcept {
    if (_keyHelper.isPrintable(keycode)) {
        ignore = _handleInteraction(Interaction::NormalInput, _keyHelper.toPrintable(keycode));
        _isSelecting.store(false);
        return;
    }

    if (const auto keyCombinationOpt = _keyHelper.fromKeycode(keycode);
        keyCombinationOpt.has_value()) {
        const auto configManager = ConfigManager::GetInstance();
        const auto [key, modifiers] = keyCombinationOpt.value();
        if (configManager->checkCommit(key, modifiers)) {
            WebsocketManager::GetInstance()->send(EditorCommitClientMessage(
                MemoryManipulator::GetInstance()->getCurrentFilePath()
            ));
            return;
        }
        if (configManager->checkManualCompletion(key, modifiers)) {
            ignore = _handleInteraction(Interaction::EnterInput);
            _isSelecting.store(false);
            return;
        }
        try {
            if (modifiers.empty()) {
                switch (key) {
                    case Key::BackSpace: {
                        ignore = _handleInteraction(Interaction::DeleteInput);
                        _isSelecting.store(false);
                        break;
                    }
                    case Key::Enter: {
                        ignore = _handleInteraction(Interaction::EnterInput);
                        _isSelecting.store(false);
                        break;
                    }
                    case Key::Home:
                    case Key::End:
                    case Key::PageDown:
                    case Key::PageUp:
                    case Key::Left:
                    case Key::Up:
                    case Key::Right:
                    case Key::Down: {
                        _navigateWithKey.store(key);
                        _isSelecting.store(false);
                        // ignore = _handleInteraction(Interaction::SelectionClear);
                        break;
                    }
                    default: {
                        // TODO: Support Key::Delete
                        break;
                    }
                }
            } else {
                if (modifiers.size() == 1) {
                    if (modifiers.contains(Modifier::Ctrl)) {
                        switch (key) {
                            case Key::S: {
                                ignore = _handleInteraction(Interaction::Save);
                                break;
                            }
                            case Key::V: {
                                ignore = _handleInteraction(Interaction::Paste);
                                break;
                            }
                            case Key::Z: {
                                ignore = _handleInteraction(Interaction::Undo);
                                break;
                            }
                            default: {
                                break;
                            }
                        }
                    }
                }
            }
        } catch (exception& e) {
            string modifierString;
            for (const auto& modifier: modifiers) {
                modifierString += format("{} + ", enum_name(modifier));
            }
            logger::warn(format(
                "Exception when processing key combination '{}{}': {}",
                modifierString,
                enum_name(key),
                e.what()
            ));
        }
    }
}

bool InteractionMonitor::_handleInteraction(const Interaction interaction, const any& data) const noexcept {
    bool needBlockMessage{false};
    try {
        for (const auto& handler: _handlerMap.at(interaction)) {
            bool handlerNeedBlockMessage{false};
            handler(data, handlerNeedBlockMessage);
            needBlockMessage |= handlerNeedBlockMessage;
        }
    } catch (out_of_range& e) {
        logger::log(format(
            "No instant handlers for interaction '{}'\n"
            "\tError: {}",
            enum_name(interaction),
            e.what()
        ));
    } catch (exception& e) {
        logger::log(format(
            "Exception when processing instant interaction '{}' : {}",
            enum_name(interaction),
            e.what()
        ));
    }
    return needBlockMessage;
}

bool InteractionMonitor::_processKeyMessage(const unsigned wParam, const unsigned lParam) {
    if (!WindowManager::GetInstance()->getCurrentWindowHandle().has_value()) {
        return false;
    }

    auto needUpdateReleaseFlag = false;
    if (!_needReleaseInteractionLock.load()) {
        logger::debug("Try locking interaction mutex");
        _interactionMutex.lock();
        needUpdateReleaseFlag = true;
        logger::debug("Interaction mutex locked");
    }

    const auto keyFlags = HIWORD(lParam);
    const auto isKeyUp = (keyFlags & KF_UP) == KF_UP;
    bool needBlockMessage{false};

    switch (wParam) {
        case VK_RETURN: {
            if (isKeyUp) {
                needBlockMessage = true;
            } else if (WindowManager::GetInstance()->hasPopListWindow()) {
                ignore = _handleInteraction(Interaction::CompletionCancel, true);
            }
            break;
        }
        case VK_ESCAPE: {
            if (isKeyUp) {
                needBlockMessage = true;
            } else if (!WindowManager::GetInstance()->hasPopListWindow()) {
                ignore = _handleInteraction(Interaction::CompletionCancel, false);
            }
            _isSelecting.store(false);
            break;
        }
        case VK_TAB: {
            if (isKeyUp) {
                needBlockMessage = true;
            } else if (WindowManager::GetInstance()->hasPopListWindow()) {
                ignore = _handleInteraction(Interaction::CompletionCancel, true);
            } else {
                needBlockMessage = _handleInteraction(Interaction::CompletionAccept);
            }
            break;
        }
        default: {
            break;
        }
    }

    _releaseInteractionLockTime.store(chrono::high_resolution_clock::now());
    if (needUpdateReleaseFlag) {
        _needReleaseInteractionLock.store(true);
    }

    return needBlockMessage;
}

void InteractionMonitor::_processMouseMessage(const unsigned wParam) {
    switch (wParam) {
        case WM_LBUTTONDOWN: {
            if (!_isMouseLeftDown.load()) {
                _isMouseLeftDown.store(true);
            }
            _navigateWithMouse.store(Mouse::Left);
            break;
        }
        case WM_MOUSEMOVE: {
            if (_isMouseLeftDown.load()) {
                _isSelecting.store(true);
            }
            break;
        }
        case WM_LBUTTONUP: {
            // if (_isSelecting.load()) {
            //     auto selectRange = _monitorCursorSelect();
            //     ignore = _handleInteraction(Interaction::SelectionSet, selectRange);
            // } else {
            //     ignore = _handleInteraction(Interaction::SelectionClear);
            // }
            _isMouseLeftDown.store(false);
            break;
        }
        case WM_LBUTTONDBLCLK: {
            _isSelecting.store(true);
            break;
        }
        default: {
            break;
        }
    }
}

void InteractionMonitor::_processWindowMessage(const long lParam) {
    const auto windowProcData = reinterpret_cast<PCWPSTRUCT>(lParam);
    if (const auto editorWindowHandle = reinterpret_cast<int64_t>(windowProcData->hwnd);
        window::getWindowClassName(editorWindowHandle) == "si_Sw") {
        switch (windowProcData->message) {
            case UM_KEYCODE: {
                _handleKeycode(windowProcData->wParam);
                break;
            }
            case WM_KILLFOCUS: {
                // logger::debug("WM_KILLFOCUS");
                if (WindowManager::GetInstance()->checkNeedHideWhenLostFocus(windowProcData->wParam)) {
                    WebsocketManager::GetInstance()->send(EditorFocusStateClientMessage(false));
                }
                break;
            }
            case WM_SETFOCUS: {
                // logger::debug("WM_SETFOCUS");
                if (WindowManager::GetInstance()->checkNeedShowWhenGainFocus(editorWindowHandle)) {
                    WebsocketManager::GetInstance()->send(EditorFocusStateClientMessage(true));
                }
                break;
            }
            default: {
                break;
            }
        }
    }
}

void InteractionMonitor::_threadMonitorCaretPosition() {
    thread([this] {
        while (_isRunning.load()) {
            if (const auto navigationBufferOpt = _navigateWithKey.load();
                navigationBufferOpt.has_value()) {
                ignore = _handleInteraction(Interaction::NavigateWithKey, navigationBufferOpt.value());
                _navigateWithKey.store(nullopt);
                continue;
            }

            // TODO: Check if need InteractionMonitor::GetInstance()->getInteractionLock();
            auto newCursorPosition = MemoryManipulator::GetInstance()->getCaretPosition();
            newCursorPosition.maxCharacter = newCursorPosition.character;
            if (const auto oldCursorPosition = _currentCaretPosition.load();
                oldCursorPosition != newCursorPosition) {
                _currentCaretPosition.store(newCursorPosition);
                if (const auto navigateWithMouseOpt = _navigateWithMouse.load();
                    navigateWithMouseOpt.has_value()) {
                    _handleInteraction(Interaction::NavigateWithMouse,
                                       make_tuple(newCursorPosition, oldCursorPosition));
                    _navigateWithMouse.store(nullopt);
                }
            }
            this_thread::sleep_for(10ms);
        }
    }).detach();
}

void InteractionMonitor::_threadReleaseInteractionLock() {
    thread([this] {
        while (_isRunning.load()) {
            if (_needReleaseInteractionLock.load()) {
                if (const auto releaseInteractionLockTime = _releaseInteractionLockTime.load();
                    chrono::high_resolution_clock::now() - releaseInteractionLockTime > 200ms) {
                    _needReleaseInteractionLock.store(false);
                    _interactionMutex.unlock();
                    logger::debug("Interaction mutex unlocked");
                }
            }
            this_thread::sleep_for(10ms);
        }
    }).detach();
}
