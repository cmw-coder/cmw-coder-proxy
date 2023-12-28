#include <format>
#include <regex>

#include <magic_enum.hpp>

#include <components/CompletionManager.h>
#include <components/Configurator.h>
#include <components/InteractionMonitor.h>
#include <components/WindowManager.h>
#include <types/SiVersion.h>
#include <utils/crypto.h>
#include <utils/inputbox.h>
#include <utils/logger.h>
#include <utils/system.h>
#include <utils/window.h>

#include <windows.h>

#include "ModificationManager.h"

using namespace components;
using namespace magic_enum;
using namespace std;
using namespace types;
using namespace utils;

namespace {
    template<class T>
    using VersionMap = unordered_map<SiVersion::Major, unordered_map<SiVersion::Minor, T>>;

    const VersionMap<tuple<uint64_t, uint64_t>> addressMap = {
        {
            SiVersion::Major::V35, {
                {SiVersion::Minor::V0076, {0x1CBEFC, 0x1CBF00}},
                {SiVersion::Minor::V0086, {0x1CD3DC, 0x1CD3E0}}
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
    // startLineOffset, startCharOffset, endLineOffset, endCharOffset
    const VersionMap<tuple<uint64_t, uint64_t, uint64_t, uint64_t>> selectAddressMap = {
        {
            SiVersion::Major::V35, {
                {SiVersion::Minor::V0086, {0x1BE0CC, 0x1C574C, 0x1E3B9C, 0x1E3BA4}}
            }
        },
    };

    constexpr auto convertLineEndings = [](const std::string& input) {
        return regex_replace(input, regex(R"(\\r\\n)"), "\r\n");
    };
    constexpr auto convertPathSeperators = [](const std::string& input) {
        return regex_replace(input, regex(R"(\\\\)"), "/");
    };

    constexpr auto autoCompletionKey = "CMWCODER_autoCompletion";
    constexpr auto currentPrefixKey = "CMWCODER_currentPrefix";
    constexpr auto cursorKey = "CMWCODER_cursor";
    constexpr auto debugLogKey = "CMWCODER_debugLog";
    constexpr auto pathKey = "CMWCODER_path";
    constexpr auto prefixKey = "CMWCODER_prefix";
    constexpr auto projectKey = "CMWCODER_project";
    constexpr auto suffixKey = "CMWCODER_suffix";
    constexpr auto symbolsKey = "CMWCODER_symbols";
    constexpr auto tabsKey = "CMWCODER_tabs";
    constexpr auto versionKey = "CMWCODER_version";
}

InteractionMonitor::InteractionMonitor()
    : _subKey(Configurator::GetInstance()->version().first == SiVersion::Major::V35
                  ? R"(SOFTWARE\Source Dynamics\Source Insight\3.0)"
                  : R"(SOFTWARE\Source Dynamics\Source Insight\4.0)"),
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
        const auto [
            startLineOffset,
            startCharOffset,
            endLineOffset,
            endCharOffset
        ] = selectAddressMap.at(majorVersion).at(minorVersion);
        _cursorStartLineAddress = baseAddress + startLineOffset;
        _cursorStartCharAddress = baseAddress + startCharOffset;
        _cursorEndLineAddress = baseAddress + endLineOffset;
        _cursorEndCharAddress = baseAddress + endCharOffset;
    } catch (out_of_range& e) {
        throw runtime_error(format("Unsupported Source Insight Version: ", e.what()));
    }

    _monitorAutoCompletion();
    _monitorCursorPosition();
    _monitorDebugLog();
    _monitorEditorInfo();
}

InteractionMonitor::~InteractionMonitor() {
    _isRunning.store(false);
}

void InteractionMonitor::_handleKeycode(const Keycode keycode) noexcept {
    if (_keyHelper.isPrintable(keycode)) {
        (void) WindowManager::GetInstance()->sendDoubleInsert();
        _handleInstantInteraction(Interaction::NormalInput, _keyHelper.toPrintable(keycode));
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
                        (void) WindowManager::GetInstance()->sendDoubleInsert();
                        _handleInstantInteraction(Interaction::DeleteInput);
                        _isSelecting.store(false);
                        break;
                    }
                    case Key::Tab: {
                        _handleInstantInteraction(Interaction::AcceptCompletion);
                        // Keep expanding when _isSelecting == true
                        break;
                    }
                    case Key::Enter: {
                        _handleInstantInteraction(Interaction::EnterInput);
                        _isSelecting.store(false);
                        break;
                    }
                    case Key::Escape: {
                        _isSelecting.store(false);
                        if (Configurator::GetInstance()->version().first == SiVersion::Major::V40) {
                            thread([this] {
                                this_thread::sleep_for(chrono::milliseconds(150));
                                _handleInstantInteraction(Interaction::CancelCompletion, make_tuple(false, true));
                            }).detach();
                        } else {
                            _handleInstantInteraction(Interaction::CancelCompletion, make_tuple(false, true));
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
                        _navigateBuffer.store(key);
                        _isSelecting.store(false);
                        _handleInstantInteraction(Interaction::ClearSelect);
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
                            ModificationManager::GetInstance()->reloadTab();
                            _handleInstantInteraction(Interaction::Save);
                            break;
                        }
                        case Key::V: {
                            _handleInstantInteraction(Interaction::Paste);
                            break;
                        }
                        case Key::Z: {
                            _handleInstantInteraction(Interaction::Undo);
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

void InteractionMonitor::_handleInstantInteraction(const Interaction interaction, const std::any& data) const noexcept {
    try {
        for (const auto& handler: _instantHandlers.at(interaction)) {
            handler(data);
        }
    } catch (out_of_range&) {
        logger::log(format("No instant handlers for interaction '{}'", enum_name(interaction)));
    }
    catch (exception& e) {
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

void InteractionMonitor::_monitorCursorPosition() {
    thread([this] {
        while (_isRunning.load()) {
            if (const auto navigationBufferOpt = _navigateBuffer.load();
                navigationBufferOpt.has_value()) {
                _handleInstantInteraction(Interaction::Navigate, navigationBufferOpt.value());
                _navigateBuffer.store(nullopt);
                continue;
            }

            CaretPosition newCursorPosition{};
            ReadProcessMemory(
                _processHandle.get(),
                reinterpret_cast<LPCVOID>(_cursorLineAddress),
                &newCursorPosition.line,
                sizeof(newCursorPosition.line),
                nullptr
            );
            ReadProcessMemory(
                _processHandle.get(),
                reinterpret_cast<LPCVOID>(_cursorCharAddress),
                &newCursorPosition.character,
                sizeof(newCursorPosition.character),
                nullptr
            );
            newCursorPosition.maxCharacter = newCursorPosition.character;
            if (const auto oldCursorPosition = _currentCursorPosition.load();
                oldCursorPosition != newCursorPosition) {
                this->_currentCursorPosition.store(newCursorPosition);
                _handleInstantInteraction(Interaction::CaretUpdate, make_tuple(newCursorPosition, oldCursorPosition));
            }
        }
    }).detach();
}

Range InteractionMonitor::_monitorCursorSelect() const {
    Range select;
    ReadProcessMemory(
        _processHandle.get(),
        reinterpret_cast<LPCVOID>(_cursorStartLineAddress),
        &select.start.line,
        sizeof(select.start.line),
        nullptr
    );
    ReadProcessMemory(
        _processHandle.get(),
        reinterpret_cast<LPCVOID>(_cursorStartCharAddress),
        &select.start.character,
        sizeof(select.start.character),
        nullptr
    );
    ReadProcessMemory(
        _processHandle.get(),
        reinterpret_cast<LPCVOID>(_cursorEndLineAddress),
        &select.end.line,
        sizeof(select.end.line),
        nullptr
    );
    ReadProcessMemory(
        _processHandle.get(),
        reinterpret_cast<LPCVOID>(_cursorEndCharAddress),
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
            if (const auto currentLinePrefixOpt = system::getEnvironmentVariable(currentPrefixKey);
                currentLinePrefixOpt.has_value()) {
                system::setEnvironmentVariable(currentPrefixKey);
                CompletionManager::GetInstance()->retrieveWithCurrentPrefix(
                    convertLineEndings(currentLinePrefixOpt.value())
                );
            } else {
                optional<string> suffixOpt, prefixOpt;
                if (Configurator::GetInstance()->version().first == SiVersion::Major::V35) {
                    suffixOpt = system::getEnvironmentVariable(suffixKey);
                    prefixOpt = system::getEnvironmentVariable(prefixKey);
                } else {
                    suffixOpt = system::getRegValue(_subKey, suffixKey);
                    prefixOpt = system::getRegValue(_subKey, prefixKey);
                }
                const auto projectOpt = system::getEnvironmentVariable(projectKey);
                const auto pathOpt = system::getEnvironmentVariable(pathKey);
                const auto cursorStringOpt = system::getEnvironmentVariable(cursorKey);
                const auto symbolStringOpt = system::getEnvironmentVariable(symbolsKey);
                const auto tabStringOpt = system::getEnvironmentVariable(tabsKey);
                if (const auto versionOpt = system::getEnvironmentVariable(versionKey);
                    suffixOpt.has_value() &&
                    prefixOpt.has_value() &&
                    projectOpt.has_value() &&
                    pathOpt.has_value() &&
                    cursorStringOpt.has_value() &&
                    versionOpt.has_value()
                ) {
                    if (Configurator::GetInstance()->version().first == SiVersion::Major::V35) {
                        system::setEnvironmentVariable(suffixKey);
                        system::setEnvironmentVariable(prefixKey);
                    } else {
                        system::deleteRegValue(_subKey, suffixKey);
                        system::deleteRegValue(_subKey, prefixKey);
                    }
                    system::setEnvironmentVariable(projectKey);
                    system::setEnvironmentVariable(pathKey);
                    system::setEnvironmentVariable(cursorKey);
                    system::setEnvironmentVariable(symbolsKey);
                    system::setEnvironmentVariable(versionKey);
                    system::setEnvironmentVariable(tabsKey);

                    _retrieveProjectId(convertPathSeperators(projectOpt.value()));
                    CompletionManager::GetInstance()->setVersion(versionOpt.value());

                    CompletionManager::GetInstance()->retrieveWithFullInfo({
                        .cursorString = cursorStringOpt.value(),
                        .path = convertPathSeperators(pathOpt.value()),
                        .prefix = convertLineEndings(prefixOpt.value()),
                        .suffix = convertLineEndings(suffixOpt.value()),
                        .symbolString = symbolStringOpt.value_or(""),
                        .tabString = tabStringOpt.value_or("")
                    });
                }
            }
            this_thread::sleep_for(chrono::milliseconds(5));
        }
    }).detach();
}

void InteractionMonitor::_processWindowMessage(const long lParam) {
    const auto windowProcData = reinterpret_cast<PCWPSTRUCT>(lParam);
    if (const auto currentWindow = reinterpret_cast<int64_t>(windowProcData->hwnd);
        window::getWindowClassName(currentWindow) == "si_Sw") {
        switch (windowProcData->message) {
            case WM_KILLFOCUS: {
                if (WindowManager::GetInstance()->checkNeedCancelWhenLostFocus(windowProcData->wParam)) {
                    _handleInstantInteraction(Interaction::CancelCompletion, make_tuple(false, true));
                }
                break;
            }
            case WM_MOUSEACTIVATE: {
                logger::debug("WM_MOUSEACTIVATE");
                _isSelecting.store(false);
                // TODO: Move this to _processWindowMouse
                _handleInstantInteraction(Interaction::Navigate);
                break;
            }
            case WM_SETFOCUS: {
                if (WindowManager::GetInstance()->checkNeedCancelWhenGainFocus(currentWindow)) {
                    _handleInstantInteraction(Interaction::CancelCompletion, make_tuple(false, true));
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
            logger::debug("WM_LBUTTONDOWN");
            if (!isLMDown.load()) {
                isLMDown.store(true);
            }
            break;
        }
        case WM_MOUSEMOVE: {
            if (isLMDown.load()) {
                logger::debug("WM_MOUSESELECT");
                _isSelecting.store(true);
            }
            break;
        }
        case WM_LBUTTONUP: {
            logger::debug("WM_LBUTTONUP");
            if (_isSelecting.load()) {
                auto selectRange = _monitorCursorSelect();
                _handleInstantInteraction(Interaction::Select, selectRange);
            } else {
                _handleInstantInteraction(Interaction::ClearSelect);
            }
            isLMDown.store(false);
            break;
        }
        case WM_LBUTTONDBLCLK: {
            _isSelecting.store(true);
            logger::debug("WM_LBUTTONDBLCLK");
            break;
        }
        default: {
            break;
        }
    }
}


void InteractionMonitor::_retrieveProjectId(const std::string& project) const {
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
