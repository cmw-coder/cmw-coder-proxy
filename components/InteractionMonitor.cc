#include <format>
#include <regex>

#include <magic_enum.hpp>

#include <components/InteractionMonitor.h>
#include <components/MemoryManipulator.h>
#include <components/WebsocketManager.h>
#include <components/WindowManager.h>
#include <utils/common.h>
#include <utils/iconv.h>
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

    char getNormalInputKey(const uint32_t virtualKeyCode, const ModifierSet& modifiers) {
        const auto scanCode = MapVirtualKey(virtualKeyCode, MAPVK_VK_TO_VSC);
        vector<BYTE> currentKeyboardState;
        currentKeyboardState.resize(256);
        if (modifiers.contains(Modifier::Shift)) {
            currentKeyboardState[VK_SHIFT] = 0x80;
        }
        WORD buffer;
        if (ToAscii(virtualKeyCode, scanCode, currentKeyboardState.data(), &buffer, 0)) {
            return LOBYTE(buffer);
        }
        return 0;
    }
}

InteractionMonitor::InteractionMonitor()
    : _configCommit({'K', {Modifier::Alt, Modifier::Ctrl}}),
      _configManualCompletion({VK_RETURN, {Modifier::Alt}}),
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

void InteractionMonitor::updateShortcutConfig(const ShortcutConfig& shortcutConfig) {
    if (shortcutConfig.commit.has_value()) {
        _configCommit.store(shortcutConfig.commit.value());
        logger::log("Update commit shortcut");
    }
    if (shortcutConfig.manualCompletion.has_value()) {
        _configManualCompletion.store(shortcutConfig.manualCompletion.value());
        logger::log("Update manual completion shortcut");
    }
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

void InteractionMonitor::_handleMouseButtonUp() {
    const auto memoryManipulator = MemoryManipulator::GetInstance();
    const auto path = memoryManipulator->getCurrentFilePath();
    const auto currentFileHandle = memoryManipulator->getHandle(
        MemoryAddress::HandleType::File
    );
    if (const auto selectionOpt = memoryManipulator->getSelection();
        currentFileHandle && selectionOpt.has_value() &&
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
            _isSelecting.store(true);
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
        _isSelecting.store(false);
        WebsocketManager::GetInstance()->send(EditorSelectionClientMessage(path));
    }
}

void InteractionMonitor::_interactionLockShared() {
    if (!_needReleaseInteractionLock.load()) {
        _interactionMutex.lock_shared();
        _needReleaseInteractionLock = true;
        _releaseInteractionLockTime.store(chrono::high_resolution_clock::now());
    }
}

bool InteractionMonitor::_processKeyMessage(const uint32_t virtualKeyCode, const uint32_t lParam) {
    if (!WindowManager::GetInstance()->getCurrentWindowHandle().has_value()) {
        return false;
    }

    _interactionLockShared();

    if (common::checkHighestBit(HIWORD(lParam))) {
        return true;
    }

    if (_isSelecting.load()) {
        _isSelecting.store(false);
        WebsocketManager::GetInstance()->send(EditorSelectionClientMessage(
            MemoryManipulator::GetInstance()->getCurrentFilePath()
        ));
    }

    bool needBlockMessage{false};
    const auto modifiers = window::getModifierKeys(virtualKeyCode);
    if ((modifiers.empty() || (modifiers.size() == 1 && modifiers.contains(Modifier::Shift))) &&
        ((0x30 <= virtualKeyCode && virtualKeyCode <= 0x39) ||
         (0x41 <= virtualKeyCode && virtualKeyCode <= 0x5A) ||
         (0x60 <= virtualKeyCode && virtualKeyCode <= 0x6F) ||
         (0xBA <= virtualKeyCode && virtualKeyCode <= 0xC0) ||
         (0xDB <= virtualKeyCode && virtualKeyCode <= 0xF5))) {
        ignore = _handleInteraction(Interaction::NormalInput, getNormalInputKey(virtualKeyCode, modifiers));
        _releaseInteractionLockTime.store(chrono::high_resolution_clock::now());
        return false;
    }

    if (const auto [shortcutKey, shortcutModifiers] = _configCommit.load();
        virtualKeyCode == shortcutKey && modifiers == shortcutModifiers) {
        // TODO: Switch lock context
        WebsocketManager::GetInstance()->send(EditorCommitClientMessage(
            MemoryManipulator::GetInstance()->getCurrentFilePath()
        ));
        _releaseInteractionLockTime.store(chrono::high_resolution_clock::now());
        return true;
    }
    if (const auto [shortcutKey, shortcutModifiers] = _configManualCompletion.load();
        virtualKeyCode == shortcutKey && modifiers == shortcutModifiers) {
        ignore = _handleInteraction(Interaction::EnterInput);
        _releaseInteractionLockTime.store(chrono::high_resolution_clock::now());
        return true;
    }

    if (modifiers.size() == 1 && modifiers.contains(Modifier::Ctrl)) {
        switch (virtualKeyCode) {
            case 'S': {
                ignore = _handleInteraction(Interaction::Save);
                break;
            }
            case 'V': {
                ignore = _handleInteraction(Interaction::Paste);
                break;
            }
            case 'Z': {
                ignore = _handleInteraction(Interaction::Undo);
                break;
            }
            default: {
                break;
            }
        }
        _releaseInteractionLockTime.store(chrono::high_resolution_clock::now());
        return false;
    }

    switch (virtualKeyCode) {
        case VK_BACK: {
            ignore = _handleInteraction(Interaction::DeleteInput);
            break;
        }
        case VK_TAB: {
            needBlockMessage = _handleInteraction(Interaction::CompletionAccept);
            break;
        }
        case VK_RETURN: {
            if (WindowManager::GetInstance()->hasPopListWindow()) {
                ignore = _handleInteraction(Interaction::CompletionCancel, false);
            } else {
                ignore = _handleInteraction(Interaction::EnterInput);
            }
            break;
        }
        case VK_ESCAPE: {
            needBlockMessage = _handleInteraction(Interaction::CompletionCancel, false);
            if (needBlockMessage) {
                ignore = WindowManager::GetInstance()->sendFocus();
            }
            break;
        }
        case VK_PRIOR:
        case VK_NEXT:
        case VK_END:
        case VK_HOME:
        case VK_LEFT:
        case VK_UP:
        case VK_RIGHT:
        case VK_DOWN: {
            _navigateKeycode.store(virtualKeyCode);
            break;
        }
        case VK_DELETE: {
            ignore = _handleInteraction(Interaction::CompletionCancel, true);
            break;
        }
        default: {
            break;
        }
    }

    _releaseInteractionLockTime.store(chrono::high_resolution_clock::now());
    return needBlockMessage;
}

void InteractionMonitor::_processMouseMessage(const unsigned wParam) {
    if (!WindowManager::GetInstance()->getCurrentWindowHandle().has_value()) {
        return;
    }

    switch (wParam) {
        case WM_LBUTTONDOWN: {
            _interactionLockShared();

            _navigateWithMouse.store(Mouse::Left);

            _releaseInteractionLockTime.store(chrono::high_resolution_clock::now());
            break;
        }
        case WM_LBUTTONUP: {
            _interactionLockShared();

            _handleMouseButtonUp();

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
            case WM_KILLFOCUS: {
                if (WindowManager::GetInstance()->checkNeedHideWhenLostFocus(windowProcData->wParam)) {
                    WebsocketManager::GetInstance()->send(EditorFocusStateClientMessage(false));
                }
                _isSelecting.store(false);
                WebsocketManager::GetInstance()->send(EditorSelectionClientMessage({}));

                _releaseInteractionLockTime.store(chrono::high_resolution_clock::now());
                break;
            }
            case WM_SETFOCUS: {
                if (WindowManager::GetInstance()->checkNeedShowWhenGainFocus(editorWindowHandle)) {
                    WebsocketManager::GetInstance()->send(EditorFocusStateClientMessage(true));
                }

                _releaseInteractionLockTime.store(chrono::high_resolution_clock::now());
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
            if (const auto navigationBuffer = _navigateKeycode.load()) {
                ignore = _handleInteraction(Interaction::NavigateWithKey, navigationBuffer);
                _navigateKeycode.store(0);
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
                    _handleInteraction(
                        Interaction::NavigateWithMouse,
                        make_tuple(newCursorPosition, oldCursorPosition)
                    );
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
                }
            }
            this_thread::sleep_for(5ms);
        }
    }).detach();
}
