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

using namespace components;
using namespace magic_enum;
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

    constexpr auto convertLineEndings = [](const std::string&input) {
        return regex_replace(input, regex(R"(\\r\\n)"), "\r\n");
    };
    constexpr auto convertPathSeperators = [](const std::string&input) {
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

InteractionMonitor::InteractionMonitor() : _subKey(Configurator::GetInstance()->version().first == SiVersion::Major::V35
                                                       ? R"(SOFTWARE\Source Dynamics\Source Insight\3.0)"
                                                       : R"(SOFTWARE\Source Dynamics\Source Insight\4.0)"),
                                           _keyHelper(Configurator::GetInstance()->version().first),
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

    _monitorAutoCompletion();
    _monitorCursorPosition();
    _monitorDebugLog();
    _monitorEditorInfo();
}

InteractionMonitor::~InteractionMonitor() {
    _isRunning.store(false);
}

void InteractionMonitor::_handleKeycode(Keycode keycode) noexcept {
    if (_keyHelper.isNavigate(keycode)) {
        _handleInteraction(Interaction::Navigate);
        return;
    }

    if (_keyHelper.isPrintable(keycode)) {
        (void)WindowManager::GetInstance()->sendDoubleInsert();
        _handleInteraction(Interaction::NormalInput, keycode);
        return;
    }

    const auto keyCombinationOpt = _keyHelper.fromKeycode(keycode);
    try {
        if (keyCombinationOpt.has_value()) {
            if (const auto [key, modifiers] = keyCombinationOpt.value(); modifiers.empty()) {
                switch (key) {
                    case Key::BackSpace: {
                        (void)WindowManager::GetInstance()->sendDoubleInsert();
                        _queueInteraction(Interaction::DeleteInput);
                        break;
                    }
                    case Key::Tab: {
                        _handleInteraction(Interaction::AcceptCompletion);
                        break;
                    }
                    case Key::Enter: {
                        _queueInteraction(Interaction::EnterInput);
                        break;
                    }
                    case Key::Escape: {
                        if (Configurator::GetInstance()->version().first == SiVersion::Major::V40) {
                            thread([this] {
                                this_thread::sleep_for(chrono::milliseconds(150));
                                _handleInteraction(Interaction::CancelCompletion, make_tuple(false, true));
                            }).detach();
                        }
                        else {
                            _handleInteraction(Interaction::CancelCompletion, make_tuple(false, true));
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
    }
    catch (...) {
    }
}

void InteractionMonitor::_handleInteraction(const Interaction interaction, const std::any&data) const noexcept {
    try {
        for (const auto&handler: _handlers.at(interaction)) {
            handler(data);
        }
    }
    catch (out_of_range&) {
        logger::log(format("No handlers for interaction '{}'", enum_name(interaction)));
    }
    catch (exception&e) {
        logger::log(format(
            "Exception when processing interaction '{}' : {}",
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
            CursorPosition newCursorPosition{};
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
            if (const auto currentCursorPosition = _currentCursorPosition.load();
                currentCursorPosition != newCursorPosition) {
                this->_currentCursorPosition.store(newCursorPosition); {
                    shared_lock lock(_interactionQueueMutex);
                    thread([this, interactionQueue = _interactionBuffer, currentCursorPosition, newCursorPosition] {
                        for (const auto&interaction: interactionQueue) {
                            _handleInteraction(interaction, make_tuple(newCursorPosition, currentCursorPosition));
                        }
                    }).detach();
                } {
                    unique_lock lock(_interactionQueueMutex);
                    _interactionBuffer.clear();
                }
            }
            this_thread::sleep_for(chrono::milliseconds(1));
        }
    }).detach();
}

void InteractionMonitor::_monitorDebugLog() const {
    thread([this] {
        while (_isRunning.load()) {
            if (const auto debugStringOpt = system::getEnvironmentVariable(debugLogKey);
                debugStringOpt.has_value()) {
                logger::debug(format("[SI] {}", debugStringOpt.value()));
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
            }
            else {
                optional<string> suffixOpt, prefixOpt;
                if (Configurator::GetInstance()->version().first == SiVersion::Major::V35) {
                    suffixOpt = system::getEnvironmentVariable(suffixKey);
                    prefixOpt = system::getEnvironmentVariable(prefixKey);
                }
                else {
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
                    }
                    else {
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
                    _handleInteraction(Interaction::CancelCompletion, make_tuple(false, true));
                }
                break;
            }
            case WM_MOUSEACTIVATE: {
                _queueInteraction(Interaction::Navigate);
                break;
            }
            case WM_SETFOCUS: {
                if (WindowManager::GetInstance()->checkNeedCancelWhenGainFocus(currentWindow)) {
                    _handleInteraction(Interaction::CancelCompletion, make_tuple(false, true));
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

void InteractionMonitor::_queueInteraction(const Interaction interaction) {
    unique_lock lock(_interactionQueueMutex);
    _interactionBuffer.emplace(interaction);
}

void InteractionMonitor::_retrieveProjectId(const std::string&project) const {
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
        }
        else {
            system::setRegValue(projectListKey, projectHash, projectId);
        }
    }

    CompletionManager::GetInstance()->setProjectId(projectId);
}

long InteractionMonitor::_windowProcedureHook(const int nCode, const unsigned int wParam, const long lParam) {
    GetInstance()->_processWindowMessage(lParam);
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}
