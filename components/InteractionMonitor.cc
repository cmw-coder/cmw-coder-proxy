#include <format>
#include <regex>

#include <magic_enum/magic_enum.hpp>

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
    : _cbtHookHandle(
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
      ),
      _configCommit({'K', {Modifier::Alt, Modifier::Ctrl}}),
      _configManualCompletion({VK_RETURN, {Modifier::Alt}}) {
    if (!_processHandle) {
        logger::error("Failed to get Source Insight's process handle");
        abort();
    }
    if (!_windowHookHandle) {
        logger::error("Failed to set window hook.");
        abort();
    }

    _threadAutoSave();
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

void InteractionMonitor::updateGenericConfig(const GenericConfig& genericConfig) {
    if (const auto autoSaveIntervalOpt = genericConfig.autoSaveInterval;
        autoSaveIntervalOpt.has_value()) {
        const auto autoSaveInterval = autoSaveIntervalOpt.value();
        logger::info(format("Update auto-save interval: {}", autoSaveInterval));
        _configAutoSaveInterval.store(autoSaveInterval);
    }
    if (const auto interactionUnlockDelayOpt = genericConfig.interactionUnlockDelay;
        interactionUnlockDelayOpt.has_value()) {
        const auto interactionUnlockDelay = interactionUnlockDelayOpt.value();
        logger::info(format("Update interaction unlock delay: {}", interactionUnlockDelay));
        _configInteractionUnlockDelay.store(interactionUnlockDelay);
    }
}

void InteractionMonitor::updateShortcutConfig(const ShortcutConfig& shortcutConfig) {
    if (const auto configOpt = shortcutConfig.commit;
        configOpt.has_value()) {
        logger::info(format("Update commit shortcut: {}", stringifyKeyCombination(configOpt.value())));
        unique_lock lock(_configCommitMutex);
        _configCommit = configOpt.value();
    }
    if (const auto configOpt = shortcutConfig.manualCompletion;
        configOpt.has_value()) {
        logger::info(format("Update manual-completion shortcut: {}", stringifyKeyCombination(configOpt.value())));
        unique_lock lock(_configManualCompletionMutex);
        _configManualCompletion = configOpt.value();
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
    const auto websocketManager = WebsocketManager::GetInstance();
    const auto path = memoryManipulator->getCurrentFilePath();
    const auto currentFileHandle = memoryManipulator->getHandle(
        MemoryAddress::HandleType::File
    );
    if (const auto selection = memoryManipulator->getSelection();
        currentFileHandle && !selection.isEmpty() &&
        selection.end.line - selection.begin.line > 2) {
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
            websocketManager->send(EditorSelectionClientMessage(
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
        websocketManager->send(EditorSelectionClientMessage(path));
    }
}

void InteractionMonitor::_handleSelectionReplace(const Selection& selection, const int32_t offsetCount) const {
    if (selection.isEmpty()) {
        return;
    }
    ignore = _handleInteraction(
        Interaction::SelectionReplace,
        make_pair(selection.begin.line, static_cast<int32_t>(selection.begin.line - selection.end.line + offsetCount))
    );
}

void InteractionMonitor::_interactionLockShared() {
    if (!_needUnlockInteraction.load()) {
        _interactionMutex.lock_shared();
        _needUnlockInteraction = true;
        _interactionUnlockTime.store(chrono::high_resolution_clock::now());
    }
}

bool InteractionMonitor::_processKeyMessage(const uint32_t virtualKeyCode, const uint32_t lParam) {
    if (!WindowManager::GetInstance()->getCurrentWindowHandle().has_value()) {
        return false;
    }
    const auto memoryManipulator = MemoryManipulator::GetInstance();
    const auto websocketManager = WebsocketManager::GetInstance();

    _interactionLockShared();

    if (common::checkHighestBit(HIWORD(lParam))) {
        return true;
    }

    if (_isSelecting.load()) {
        _isSelecting.store(false);
        websocketManager->send(EditorSelectionClientMessage(memoryManipulator->getCurrentFilePath()));
    }

    bool needBlockMessage{false};
    const auto modifiers = window::getModifierKeys(virtualKeyCode);
    if ((modifiers.empty() || (modifiers.size() == 1 && modifiers.contains(Modifier::Shift))) &&
        ((0x30 <= virtualKeyCode && virtualKeyCode <= 0x39) ||
         (0x41 <= virtualKeyCode && virtualKeyCode <= 0x5A) ||
         (0x60 <= virtualKeyCode && virtualKeyCode <= 0x6F) ||
         (0xBA <= virtualKeyCode && virtualKeyCode <= 0xC0) ||
         (0xDB <= virtualKeyCode && virtualKeyCode <= 0xF5))) {
        _handleSelectionReplace(memoryManipulator->getSelection());
        ignore = _handleInteraction(Interaction::NormalInput, getNormalInputKey(virtualKeyCode, modifiers));
        _interactionUnlockTime.store(chrono::high_resolution_clock::now());
        return false;
    }

    KeyCombination configCommit, configManualCompletion; {
        shared_lock lock(_configCommitMutex);
        configCommit = _configCommit;
    } {
        shared_lock lock(_configManualCompletionMutex);
        configManualCompletion = _configManualCompletion;
    }

    if (const auto [shortcutKey, shortcutModifiers] = configCommit;
        virtualKeyCode == shortcutKey && modifiers == shortcutModifiers) {
        // TODO: Switch lock context
        websocketManager->send(EditorCommitClientMessage(memoryManipulator->getCurrentFilePath()));
        _interactionUnlockTime.store(chrono::high_resolution_clock::now());
        return true;
    }
    if (const auto [shortcutKey, shortcutModifiers] = configManualCompletion;
        virtualKeyCode == shortcutKey && modifiers == shortcutModifiers) {
        ignore = _handleInteraction(Interaction::EnterInput);
        _interactionUnlockTime.store(chrono::high_resolution_clock::now());
        return true;
    }

    if (modifiers.size() == 1 && modifiers.contains(Modifier::Ctrl)) {
        switch (virtualKeyCode) {
            case 'S': {
                ignore = _handleInteraction(Interaction::Save);
                break;
            }
            case 'V': {
                _handleSelectionReplace(memoryManipulator->getSelection());
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
        _interactionUnlockTime.store(chrono::high_resolution_clock::now());
        return false;
    }

    switch (virtualKeyCode) {
        case VK_BACK: {
            _handleSelectionReplace(memoryManipulator->getSelection());
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
                _handleSelectionReplace(memoryManipulator->getSelection(), 1);
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
            _handleSelectionReplace(memoryManipulator->getSelection());
            ignore = _handleInteraction(Interaction::CompletionCancel, true);
            break;
        }
        default: {
            break;
        }
    }

    _interactionUnlockTime.store(chrono::high_resolution_clock::now());
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

            _interactionUnlockTime.store(chrono::high_resolution_clock::now());
            break;
        }
        case WM_LBUTTONUP: {
            _interactionLockShared();

            _handleMouseButtonUp();

            _interactionUnlockTime.store(chrono::high_resolution_clock::now());
            break;
        }
        default: {
            break;
        }
    }
}

void InteractionMonitor::_processWindowMessage(const long lParam) {
    const auto windowProcData = reinterpret_cast<PCWPSTRUCT>(lParam);
    const auto windowHandle = reinterpret_cast<int64_t>(windowProcData->hwnd);
    if (const auto windowClassName = window::getWindowClassName(windowHandle);
        windowClassName == "si_Sw") {
        const auto websocketManager = WebsocketManager::GetInstance();
        switch (windowProcData->message) {
            case WM_SETFOCUS: {
                if (WindowManager::GetInstance()->checkNeedShowWhenGainFocus(windowHandle)) {
                    websocketManager->send(EditorStateClientMessage(true));
                }

                _interactionUnlockTime.store(chrono::high_resolution_clock::now());
                break;
            }
            case WM_KILLFOCUS: {
                if (WindowManager::GetInstance()->checkNeedHideWhenLostFocus(windowProcData->wParam)) {
                    websocketManager->send(EditorStateClientMessage(false));
                }
                _isSelecting.store(false);
                websocketManager->send(EditorSelectionClientMessage({}));

                _interactionUnlockTime.store(chrono::high_resolution_clock::now());
                break;
            }
            default: {
                break;
            }
        }
    } else if (windowClassName == "si_Frame") {
        const auto websocketManager = WebsocketManager::GetInstance();
        switch (windowProcData->message) {
            case WM_MOVE:
            case WM_SIZE: {
                const auto [left, top, right, bottom] = window::getWindowRect(windowHandle);
                websocketManager->send(EditorStateClientMessage({
                    .height = bottom - top,
                    .width = right - left,
                    .x = left,
                    .y = top
                }));
                break;
            }
            case WM_CLOSE: {
                logger::debug("si_Frame is destroyed.");
                websocketManager->close();
                break;
            }
            default: {
                break;
            }
        }
    }
}

void InteractionMonitor::_threadAutoSave() const {
    thread([this] {
        Time lastSaveTime = chrono::high_resolution_clock::now();
        while (_isRunning.load()) {
            if (const auto autoSaveInterval = chrono::seconds(_configAutoSaveInterval.load());
                chrono::high_resolution_clock::now() - lastSaveTime > autoSaveInterval) {
                WindowManager::GetInstance()->sendSave();
                lastSaveTime = chrono::high_resolution_clock::now();
            }
            this_thread::sleep_for(1s);
        }
    }).detach();
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
            this_thread::sleep_for(100ms);
        }
    }).detach();
}

void InteractionMonitor::_threadReleaseInteractionLock() {
    thread([this] {
        while (_isRunning.load()) {
            if (_needUnlockInteraction.load()) {
                if (chrono::high_resolution_clock::now() - _interactionUnlockTime.load() >
                    chrono::milliseconds(_configInteractionUnlockDelay)) {
                    _needUnlockInteraction.store(false);
                    _interactionMutex.unlock_shared();
                }
            }
            this_thread::sleep_for(10ms);
        }
    }).detach();
}
