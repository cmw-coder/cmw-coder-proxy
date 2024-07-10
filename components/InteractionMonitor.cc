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
#include <utils/common.h>
#include <utils/iconv.h>

using namespace components;
using namespace magic_enum;
using namespace models;
using namespace std;
using namespace types;
using namespace utils;

namespace {
    const auto mainThreadId = system::getMainThreadId(GetCurrentProcessId());

    optional<tuple<string, string>> getBlockContext(
        const uint32_t fileHandle,
        const uint32_t beginLine,
        const uint32_t endLine
    ) {
        const auto memoryManipulator = MemoryManipulator::GetInstance();
        string blockPrefix, blockSuffix;
        bool isInBlock{false};
        for (uint32_t index = 1; index <= min(beginLine, 200u); ++index) {
            auto tempLine = iconv::autoDecode(memoryManipulator->getLineContent(
                fileHandle, beginLine - index
            ));
            if (tempLine[0] == '}' || regex_search(tempLine, regex(R"~(^\/\*\*)~"))) {
                break;
            }
            if (regex_search(tempLine, regex(R"~(\*\*\/$)~"))) {
                isInBlock = true;
                break;
            }
            blockPrefix.insert(0, tempLine.append("\n"));
        }
        if (!isInBlock) {
            return nullopt;
        }

        isInBlock = false;
        for (uint32_t index = 1; index <= 200u; ++index) {
            const auto tempLine = iconv::autoDecode(memoryManipulator->getLineContent(
                fileHandle, endLine + index
            ));
            if (regex_search(tempLine, regex(R"~(\*\*\/$)~"))) {
                break;
            }
            if (regex_search(tempLine, regex(R"~(^\/\*\*)~"))) {
                isInBlock = true;
                break;
            }
            blockSuffix.append("\n").append(tempLine);
            if (tempLine[0] == '}') {
                isInBlock = true;
                break;
            }
        }
        if (!isInBlock) {
            return nullopt;
        }
        return make_tuple(blockPrefix, blockSuffix);
    }
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

unique_lock<shared_mutex> InteractionMonitor::getInteractionLock() const {
    return unique_lock(_interactionMutex);
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
            return;
        }
        try {
            if (modifiers.empty()) {
                switch (key) {
                    case Key::BackSpace: {
                        ignore = _handleInteraction(Interaction::DeleteInput);
                        break;
                    }
                    case Key::Enter: {
                        ignore = _handleInteraction(Interaction::EnterInput);
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

void InteractionMonitor::_interactionLockShared() {
    if (!_needReleaseInteractionLock.load()) {
        _interactionMutex.lock_shared();
        _needReleaseInteractionLock = true;
        _releaseInteractionLockTime.store(chrono::high_resolution_clock::now());
        // logger::debug("[_processKeyMessage] Successfuly got interaction shared lock");
    }
}

bool InteractionMonitor::_processKeyMessage(const unsigned wParam, const unsigned lParam) {
    if (!WindowManager::GetInstance()->getCurrentWindowHandle().has_value()) {
        return false;
    }

    _interactionLockShared();

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
            _releaseInteractionLockTime.store(chrono::high_resolution_clock::now());
            break;
        }
        case VK_ESCAPE: {
            if (isKeyUp) {
                needBlockMessage = true;
            } else if (!WindowManager::GetInstance()->hasPopListWindow()) {
                ignore = _handleInteraction(Interaction::CompletionCancel, false);
            }
            _releaseInteractionLockTime.store(chrono::high_resolution_clock::now());
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
            _releaseInteractionLockTime.store(chrono::high_resolution_clock::now());
            break;
        }
        default: {
            break;
        }
    }

    return needBlockMessage;
}

void InteractionMonitor::_processMouseMessage(const unsigned wParam) {
    if (!WindowManager::GetInstance()->getCurrentWindowHandle().has_value()) {
        return;
    }

    switch (wParam) {
        case WM_LBUTTONDOWN: {
            _interactionLockShared();

            if (!_isMouseLeftDown.load()) {
                _isMouseLeftDown.store(true);
            }
            _navigateWithMouse.store(Mouse::Left);
            _releaseInteractionLockTime.store(chrono::high_resolution_clock::now());
            break;
        }
        case WM_LBUTTONUP: {
            _interactionLockShared();
            const auto memoryManipulator = MemoryManipulator::GetInstance();
            const auto path = memoryManipulator->getCurrentFilePath();
            const auto currentFileHandle = memoryManipulator->getHandle(MemoryAddress::HandleType::File);
            if (const auto selectionOpt = memoryManipulator->getSelection();
                currentFileHandle &&
                selectionOpt.has_value() &&
                selectionOpt.value().end.line - selectionOpt.value().begin.line > 2) {
                const auto selection = selectionOpt.value();
                if (const auto [height, xPosition, yPosition] = common::getCaretDimensions(false);
                    height) {
                    string selectionBlock, selectionContent;
                    bool needFindBlockContext{true};
                    uint32_t lastLineRemovalCount{};
                    string lineContent;
                    for (uint32_t index = selection.begin.line; index <= selection.end.line; ++index) {
                        const auto currentLine = iconv::autoDecode(
                            memoryManipulator->getLineContent(currentFileHandle, index)
                        );
                        if (currentLine[0] == '{' || currentLine[0] == '}') {
                            needFindBlockContext = false;
                        }
                        if (index == selection.end.line) {
                            lastLineRemovalCount = currentLine.length() - selection.end.character;
                        }
                        if (index == selection.begin.line) {
                            lineContent.append(currentLine);
                        } else {
                            lineContent.append("\n").append(currentLine);
                        }
                    }
                    selectionContent = lineContent.substr(
                        selection.begin.character, lineContent.length() - lastLineRemovalCount
                    );
                    if (needFindBlockContext) {
                        if (const auto blockContextOpt = getBlockContext(
                            currentFileHandle, selection.begin.line, selection.end.line
                        ); blockContextOpt.has_value()) {
                            auto [blockPrefix, blockSuffix] = blockContextOpt.value();
                            selectionBlock = blockPrefix.append(lineContent).append(blockSuffix);
                        }
                    }
                    WebsocketManager::GetInstance()->send(EditorSelectionClientMessage(
                        path,
                        selectionContent,
                        selectionBlock,
                        selection,
                        height,
                        xPosition,
                        yPosition
                    ));
                }
            } else {
                WebsocketManager::GetInstance()->send(EditorSelectionClientMessage(path));
            }
            _isMouseLeftDown.store(false);
            _releaseInteractionLockTime.store(chrono::high_resolution_clock::now());
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
                _releaseInteractionLockTime.store(chrono::high_resolution_clock::now());
                break;
            }
            case WM_KILLFOCUS: {
                // logger::debug("WM_KILLFOCUS");
                if (WindowManager::GetInstance()->checkNeedHideWhenLostFocus(windowProcData->wParam)) {
                    WebsocketManager::GetInstance()->send(EditorFocusStateClientMessage(false));
                }
                WebsocketManager::GetInstance()->send(EditorSelectionClientMessage({}));
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
                    _interactionMutex.unlock_shared();
                    // logger::debug("[_processKeyMessage] Interaction mutex shared unlocked");
                }
            }
            this_thread::sleep_for(5ms);
        }
    }).detach();
}
