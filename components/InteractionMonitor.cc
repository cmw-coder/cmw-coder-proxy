#include <format>
#include <regex>

#include <magic_enum.hpp>

#include <components/CompletionManager.h>
#include <components/Configurator.h>
#include <components/InteractionMonitor.h>
#include <components/WebsocketManager.h>
#include <components/WindowManager.h>
#include <types/common.h>
#include <types/CompactString.h>
#include <types/SiVersion.h>
#include <utils/crypto.h>
#include <utils/inputbox.h>
#include <utils/logger.h>
#include <utils/memory.h>
#include <utils/system.h>
#include <utils/window.h>

#include <windows.h>

using namespace components;
using namespace magic_enum;
using namespace std;
using namespace types;
using namespace utils;

namespace {
    constexpr auto convertPathSeperators = [](const string& input) {
        return regex_replace(input, regex(R"(\\\\)"), "/");
    };

    constexpr auto autoCompletionKey = "CMWCODER_autoCompletion";
    constexpr auto debugLogKey = "CMWCODER_debugLog";
    constexpr auto projectKey = "CMWCODER_project";
    constexpr auto symbolsKey = "CMWCODER_symbols";
    constexpr auto tabsKey = "CMWCODER_tabs";
    constexpr auto versionKey = "CMWCODER_version";
}

InteractionMonitor::InteractionMonitor()
    : _subKey(Configurator::GetInstance()->version().first == SiVersion::Major::V35
                  ? R"(SOFTWARE\Source Dynamics\Source Insight\3.0)"
                  : R"(SOFTWARE\Source Dynamics\Source Insight\4.0)"),
      _baseAddress(reinterpret_cast<uint32_t>(GetModuleHandle(nullptr))),
      _keyHelper(Configurator::GetInstance()->version().first),
      _mouseHookHandle(
          SetWindowsHookEx(
              WH_MOUSE,
              _windowProcedureHook,
              GetModuleHandle(nullptr),
              GetCurrentThreadId()
          ),
          UnhookWindowsHookEx
      ),
      _processHandle(GetCurrentProcess(), CloseHandle),
      _windowHookHandle(
          SetWindowsHookEx(
              WH_CALLWNDPROC,
              _windowProcedureHook,
              GetModuleHandle(nullptr),
              GetCurrentThreadId()
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
    try {
        _memoryAddress = memory::getAddresses(Configurator::GetInstance()->version());
    } catch (runtime_error& e) {
        logger::error(e.what());
        abort();
    }

    _monitorAutoCompletion();
    _monitorCaretPosition();
    _monitorDebugLog();
    _monitorEditorInfo();
}

InteractionMonitor::~InteractionMonitor() {
    _isRunning.store(false);
}

void InteractionMonitor::deleteLineContent(const uint32_t line) const {
    if (!WindowManager::GetInstance()->hasValidCodeWindow()) {
        throw runtime_error("No valid code window");
    }

    const auto functionDelBufLine = StdCallFunction<void(uint32_t, uint32_t, uint32_t)>(
        _baseAddress + _memoryAddress.file.funcDelBufLine.funcAddress
    );

    uint32_t fileHandle;
    ReadProcessMemory(
        _processHandle.get(),
        reinterpret_cast<LPCVOID>(_baseAddress + _memoryAddress.file.fileHandle),
        &fileHandle,
        sizeof(fileHandle),
        nullptr
    );

    if (fileHandle) {
        functionDelBufLine(fileHandle, line, 1);
    }
}

tuple<int64_t, int64_t> InteractionMonitor::getCaretPixels(const uint32_t line) const {
    if (!WindowManager::GetInstance()->hasValidCodeWindow()) {
        throw runtime_error("No valid code window");
    }

    const auto hwndAddress = _baseAddress + _memoryAddress.caret.dimension.y.windowHandle;
    const auto functionYPosFromLine = StdCallFunction<uint32_t(uint32_t, uint32_t)>(
        _baseAddress + _memoryAddress.caret.dimension.y.funcYPosFromLine.funcAddress
    );
    uint32_t hwnd, xPixel;
    ReadProcessMemory(
        _processHandle.get(),
        reinterpret_cast<LPCVOID>(hwndAddress),
        &hwnd,
        sizeof(hwnd),
        nullptr
    );
    while (!hwnd) {
        this_thread::sleep_for(chrono::milliseconds(5));
        ReadProcessMemory(
            _processHandle.get(),
            reinterpret_cast<LPCVOID>(hwndAddress),
            &hwnd,
            sizeof(hwnd),
            nullptr
        );
    }
    const auto yPixel = functionYPosFromLine(hwnd, line); {
        uint32_t xPosPointer;
        ReadProcessMemory(
            _processHandle.get(),
            reinterpret_cast<LPCVOID>(_baseAddress + _memoryAddress.caret.dimension.x.pointer),
            &xPosPointer,
            sizeof(xPosPointer),
            nullptr
        );
        ReadProcessMemory(
            _processHandle.get(),
            reinterpret_cast<LPCVOID>(xPosPointer + _memoryAddress.caret.dimension.x.offset1),
            &xPixel,
            sizeof(xPixel),
            nullptr
        );
    }

    const auto [clientX, clientY] = WindowManager::GetInstance()->getCurrentPosition();
    logger::debug(format("Line {} Positions: Client ({}, {}), Caret ({}, {})", line, clientX, clientY, xPixel, yPixel));
    return {
        clientX + static_cast<int64_t>(xPixel),
        clientY + static_cast<int64_t>(yPixel)
    };
}

CaretPosition InteractionMonitor::getCaretPosition() const {
    CaretPosition cursorPosition{};
    ReadProcessMemory(
        _processHandle.get(),
        reinterpret_cast<LPCVOID>(_baseAddress + _memoryAddress.caret.position.current.line),
        &cursorPosition.line,
        sizeof(cursorPosition.line),
        nullptr
    );
    ReadProcessMemory(
        _processHandle.get(),
        reinterpret_cast<LPCVOID>(_baseAddress + _memoryAddress.caret.position.current.character),
        &cursorPosition.character,
        sizeof(cursorPosition.character),
        nullptr
    );
    return cursorPosition;
}

string InteractionMonitor::getFileName() const {
    if (!WindowManager::GetInstance()->hasValidCodeWindow()) {
        throw runtime_error("No valid code window");
    }

    uint32_t param1;
    const auto functionGetBufName = StdCallFunction<void(uint32_t, void*)>(
        _baseAddress + _memoryAddress.file.funcGetBufName.funcAddress
    ); {
        uint32_t fileHandle, param1Offset1;
        ReadProcessMemory(
            _processHandle.get(),
            reinterpret_cast<LPCVOID>(_baseAddress + _memoryAddress.file.fileHandle),
            &fileHandle,
            sizeof(fileHandle),
            nullptr
        );
        ReadProcessMemory(
            _processHandle.get(),
            reinterpret_cast<LPCVOID>(fileHandle + _memoryAddress.file.funcGetBufName.param1Offset1),
            &param1Offset1,
            sizeof(param1Offset1),
            nullptr
        );
        ReadProcessMemory(
            _processHandle.get(),
            reinterpret_cast<LPCVOID>(param1Offset1 + _memoryAddress.file.funcGetBufName.param1Offset2),
            &param1,
            sizeof(param1),
            nullptr
        );
    }

    CompactString payload;
    functionGetBufName(param1, payload.data());
    return payload.str();
}

string InteractionMonitor::getLineContent() const {
    return getLineContent(getCaretPosition().line);
}

string InteractionMonitor::getLineContent(const uint32_t line) const {
    if (!WindowManager::GetInstance()->hasValidCodeWindow()) {
        throw runtime_error("No valid code window");
    }

    const auto functionGetBufLine = StdCallFunction<void(uint32_t, uint32_t, void*)>(
        _baseAddress + _memoryAddress.file.funcGetBufLine.funcAddress
    );

    uint32_t fileHandle;
    ReadProcessMemory(
        _processHandle.get(),
        reinterpret_cast<LPCVOID>(_baseAddress + _memoryAddress.file.fileHandle),
        &fileHandle,
        sizeof(fileHandle),
        nullptr
    );

    if (fileHandle) {
        CompactString payload;
        functionGetBufLine(fileHandle, line, payload.data());
        return payload.str();
    }
    return {};
}

void InteractionMonitor::insertLineContent(const std::string& content) const {
    insertLineContent(getCaretPosition().line, content);
}

void InteractionMonitor::insertLineContent(const uint32_t line, const std::string& content) const {
    if (!WindowManager::GetInstance()->hasValidCodeWindow()) {
        throw runtime_error("No valid code window");
    }

    const auto functionInsBufLine = StdCallFunction<void(uint32_t, uint32_t, void*)>(
        _baseAddress + _memoryAddress.file.funcInsBufLine.funcAddress
    );

    uint32_t fileHandle;
    ReadProcessMemory(
        _processHandle.get(),
        reinterpret_cast<LPCVOID>(_baseAddress + _memoryAddress.file.fileHandle),
        &fileHandle,
        sizeof(fileHandle),
        nullptr
    );

    if (fileHandle) {
        CompactString payload(content);
        functionInsBufLine(fileHandle, line, payload.data());
    }
}

void InteractionMonitor::setCaretPosition(const CaretPosition& caretPosition) const {
    if (!WindowManager::GetInstance()->hasValidCodeWindow()) {
        throw runtime_error("No valid code window");
    }
    const auto SetWndSel = StdCallFunction<void(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)>(
        _baseAddress + _memoryAddress.window.funcSetWndSel.funcAddress
    );

    uint32_t hwnd;
    ReadProcessMemory(
        _processHandle.get(),
        reinterpret_cast<LPCVOID>(_baseAddress + _memoryAddress.caret.dimension.y.windowHandle),
        &hwnd,
        sizeof(hwnd),
        nullptr
    );
    if (hwnd) {
        SetWndSel(hwnd, caretPosition.line, caretPosition.character, caretPosition.line, caretPosition.character);
    }
}

void InteractionMonitor::setLineContent(const uint32_t line, const string& content) const {
    if (!WindowManager::GetInstance()->hasValidCodeWindow()) {
        throw runtime_error("No valid code window");
    }

    const auto functionPutBufLine = StdCallFunction<void(uint32_t, uint32_t, void*)>(
        _baseAddress + _memoryAddress.file.funcPutBufLine.funcAddress
    );

    uint32_t fileHandle;
    ReadProcessMemory(
        _processHandle.get(),
        reinterpret_cast<LPCVOID>(_baseAddress + _memoryAddress.file.fileHandle),
        &fileHandle,
        sizeof(fileHandle),
        nullptr
    );

    if (fileHandle) {
        CompactString payload(content);
        functionPutBufLine(fileHandle, line, payload.data());
    }
}

void InteractionMonitor::setSelectedContent(const std::string& content) const {
    if (!WindowManager::GetInstance()->hasValidCodeWindow()) {
        throw runtime_error("No valid code window");
    }

    const auto functionSetBufSelText = StdCallFunction<void(uint32_t, const char*)>(
        _baseAddress + _memoryAddress.window.funcSetBufSelText.funcAddress
    );

    uint32_t param1;
    ReadProcessMemory(
        _processHandle.get(),
        reinterpret_cast<LPCVOID>(_baseAddress + _memoryAddress.window.funcSetBufSelText.param1),
        &param1,
        sizeof(param1),
        nullptr
    );

    if (param1) {
        functionSetBufSelText(param1, content.data());
    }
}

void InteractionMonitor::_handleKeycode(const Keycode keycode) noexcept {
    if (_keyHelper.isPrintable(keycode)) {
        _handleInteraction(Interaction::NormalInput, _keyHelper.toPrintable(keycode));
        _isSelecting.store(false);
        return;
    }

    const auto keyCombinationOpt = _keyHelper.fromKeycode(keycode);
    try {
        if (keyCombinationOpt.has_value()) {
            if (const auto [key, modifiers] = keyCombinationOpt.value();
                modifiers.empty()) {
                switch (key) {
                    case Key::BackSpace: {
                        _handleInteraction(Interaction::DeleteInput);
                        _isSelecting.store(false);
                        break;
                    }
                    case Key::Tab: {
                        _handleInteraction(Interaction::AcceptCompletion);
                        // Keep expanding when _isSelecting == true
                        break;
                    }
                    case Key::Enter: {
                        _handleInteraction(Interaction::EnterInput);
                        _isSelecting.store(false);
                        break;
                    }
                    case Key::Escape: {
                        _isSelecting.store(false);
                        if (Configurator::GetInstance()->version().first == SiVersion::Major::V40) {
                            thread([this] {
                                this_thread::sleep_for(chrono::milliseconds(150));
                                _handleInteraction(Interaction::CancelCompletion, make_tuple(false, true));
                            }).detach();
                        } else {
                            _handleInteraction(Interaction::CancelCompletion, make_tuple(false, true));
                        }
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
                        _handleInteraction(Interaction::SelectionClear);
                        break;
                    }
                    default: {
                        // TODO: Support Key::Delete
                        break;
                    }
                }
            } else {
                if (modifiers.size() == 1 && modifiers.contains(Modifier::Ctrl)) {
                    switch (key) {
                        case Key::S: {
                            _handleInteraction(Interaction::Save);
                            break;
                        }
                        case Key::V: {
                            _handleInteraction(Interaction::Paste);
                            break;
                        }
                        case Key::Z: {
                            _handleInteraction(Interaction::Undo);
                            break;
                        }
                        default: {
                            break;
                        }
                    }
                }
            }
        }
    } catch (...) {}
}

void InteractionMonitor::_handleInteraction(const Interaction interaction, const any& data) const noexcept {
    try {
        for (const auto& handler: _handlerMap.at(interaction)) {
            handler(data);
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
}

void InteractionMonitor::_monitorAutoCompletion() const {
    thread([this] {
        while (_isRunning.load()) {
            if (const auto isAutoCompletionOpt = system::getRegValue(_subKey, autoCompletionKey);
                isAutoCompletionOpt.has_value()) {
                CompletionManager::GetInstance()->setAutoCompletion(
                    static_cast<bool>(stoi(isAutoCompletionOpt.value()))
                );
            }
            this_thread::sleep_for(chrono::milliseconds(25));
        }
    }).detach();
}

void InteractionMonitor::_monitorCaretPosition() {
    thread([this] {
        while (_isRunning.load()) {
            if (const auto navigationBufferOpt = _navigateWithKey.load();
                navigationBufferOpt.has_value()) {
                _handleInteraction(Interaction::NavigateWithKey, navigationBufferOpt.value());
                _navigateWithKey.store(nullopt);
                continue;
            }

            auto newCursorPosition = getCaretPosition();
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
        }
    }).detach();
}

Range InteractionMonitor::_monitorCursorSelect() const {
    Range select{};
    ReadProcessMemory(
        _processHandle.get(),
        reinterpret_cast<LPCVOID>(_baseAddress + _memoryAddress.caret.position.begin.line),
        &select.start.line,
        sizeof(select.start.line),
        nullptr
    );
    ReadProcessMemory(
        _processHandle.get(),
        reinterpret_cast<LPCVOID>(_baseAddress + _memoryAddress.caret.position.begin.character),
        &select.start.character,
        sizeof(select.start.character),
        nullptr
    );
    ReadProcessMemory(
        _processHandle.get(),
        reinterpret_cast<LPCVOID>(_baseAddress + _memoryAddress.caret.position.end.line),
        &select.end.line,
        sizeof(select.end.line),
        nullptr
    );
    ReadProcessMemory(
        _processHandle.get(),
        reinterpret_cast<LPCVOID>(_baseAddress + _memoryAddress.caret.position.end.character),
        &select.end.character,
        sizeof(select.end.character),
        nullptr
    );
    return select;
}

void InteractionMonitor::_monitorDebugLog() const {
    thread([this] {
        while (_isRunning.load()) {
            if (const auto debugStringOpt = system::getEnvironmentVariable(debugLogKey);
                debugStringOpt.has_value()) {
                logger::debug(format("[SI] {}", regex_replace(debugStringOpt.value(), regex("\\n"), "\n")));
                system::setEnvironmentVariable(debugLogKey);
            }
            this_thread::sleep_for(chrono::milliseconds(1));
        }
    }).detach();
}

void InteractionMonitor::_monitorEditorInfo() const {
    thread([this] {
        while (_isRunning.load()) {
            const auto projectOpt = system::getEnvironmentVariable(projectKey);
            const auto symbolStringOpt = system::getEnvironmentVariable(symbolsKey);
            const auto tabStringOpt = system::getEnvironmentVariable(tabsKey);
            if (const auto versionOpt = system::getEnvironmentVariable(versionKey);
                projectOpt.has_value() &&
                versionOpt.has_value()
            ) {
                system::setEnvironmentVariable(projectKey);
                system::setEnvironmentVariable(symbolsKey);
                system::setEnvironmentVariable(versionKey);
                system::setEnvironmentVariable(tabsKey);

                _retrieveProjectId(convertPathSeperators(projectOpt.value()));
                CompletionManager::GetInstance()->setVersion(versionOpt.value());
            }
            this_thread::sleep_for(chrono::milliseconds(10));
        }
    }).detach();
}

void InteractionMonitor::_processWindowMessage(const long lParam) {
    const auto windowProcData = reinterpret_cast<PCWPSTRUCT>(lParam);
    if (const auto currentWindow = reinterpret_cast<int64_t>(windowProcData->hwnd);
        window::getWindowClassName(currentWindow) == "si_Sw") {
        switch (windowProcData->message) {
            case WM_KILLFOCUS: {
                if (WindowManager::GetInstance()->checkNeedHideWhenLostFocus(windowProcData->wParam)) {
                    // WebsocketManager::GetInstance()->sendAction(WsAction::ImmersiveHide);
                }
                break;
            }
            case WM_SETFOCUS: {
                if (WindowManager::GetInstance()->checkNeedShowWhenGainFocus(currentWindow)) {
                    // WebsocketManager::GetInstance()->sendAction(WsAction::ImmersiveShow);
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

void InteractionMonitor::_processWindowMouse(const unsigned wParam) {
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
            if (_isSelecting.load()) {
                auto selectRange = _monitorCursorSelect();
                _handleInteraction(Interaction::SelectionSet, selectRange);
            } else {
                _handleInteraction(Interaction::SelectionClear);
            }
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


void InteractionMonitor::_retrieveProjectId(const string& project) const {
    const auto projectListKey = _subKey + "\\Project List";
    const auto projectHash = crypto::sha1(project);
    string projectId;

    if (const auto projectIdOpt = system::getRegValue(projectListKey, projectHash); projectIdOpt.has_value()) {
        projectId = projectIdOpt.value();
    }

    while (projectId.empty()) {
        projectId = InputBox("Please input current project's iSoft ID", "Input Project ID");
        if (projectId.empty()) {
            logger::error("Project ID is empty, please input a valid Project ID.");
        } else {
            system::setRegValue(projectListKey, projectHash, projectId);
        }
    }

    CompletionManager::GetInstance()->setProjectId(projectId);
}

long InteractionMonitor::_windowProcedureHook(const int nCode, const unsigned int wParam, const long lParam) {
    GetInstance()->_processWindowMessage(lParam);
    GetInstance()->_processWindowMouse(wParam);
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}
