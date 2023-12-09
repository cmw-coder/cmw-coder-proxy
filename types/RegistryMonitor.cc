#include <chrono>
#include <regex>

#include <magic_enum.hpp>
#include <nlohmann/json.hpp>
#include <httplib.h>

#include <types/Configurator.h>
#include <types/RegistryMonitor.h>
#include <types/UserAction.h>
#include <types/WindowInterceptor.h>
#include <utils/crypto.h>
#include <utils/inputbox.h>
#include <utils/logger.h>
#include <utils/system.h>

using namespace magic_enum;
using namespace std;
using namespace types;
using namespace utils;

namespace {
    //    const regex cursorRegex(
    //            R"regex(^lnFirst="(.*?)";ichFirst="(.*?)";lnLast="(.*?)";ichLim="(.*?)";fExtended="(.*?)";fRect="(.*?)"$)regex");
}

RegistryMonitor::RegistryMonitor() : _keyHelper(Configurator::GetInstance()->version().first),
                                     _subKey(Configurator::GetInstance()->version().first == SiVersion::Major::V35
                                                 ? R"(SOFTWARE\Source Dynamics\Source Insight\3.0)"
                                                 : R"(SOFTWARE\Source Dynamics\Source Insight\4.0)") {
    if (const auto isAutoCompletion = system::getRegValue(_subKey, "CMWCODER_autoCompletion");
        isAutoCompletion.has_value()) {
        _isAutoCompletion.store(stoi(isAutoCompletion.value()));
    }

    logger::log(format("Auto completion: {}", _isAutoCompletion.load() ? "on" : "off"));

    _threadProcessInfo();
    _threadCompletionMode();
    _threadLogDebug();
}

RegistryMonitor::~RegistryMonitor() {
    _isRunning.store(false);
}

void RegistryMonitor::acceptByTab(Keycode) {
    _isContinuousEnter.store(false);
    _isJustAccepted.store(true);
    if (auto oldCompletion = _completionCache.reset(); !oldCompletion.content().empty()) {
        WindowInterceptor::GetInstance()->sendAcceptCompletion();
        logger::log(format("Accepted completion: {}", oldCompletion.stringify()));
        thread(&RegistryMonitor::_reactToCompletion, this, move(oldCompletion), true).detach();
    }
    else {
        _retrieveEditorInfo();
    }
}

void RegistryMonitor::cancelByCursorNavigate(CursorPosition, CursorPosition) {
    cancelByKeycodeNavigate(-1);
}

void RegistryMonitor::cancelByDeleteBackward(const CursorPosition oldPosition, const CursorPosition newPosition) {
    _isContinuousEnter.store(false);
    if (oldPosition.line == newPosition.line) {
        if (const auto previousCacheOpt = _completionCache.previous(); previousCacheOpt.has_value()) {
            // Has valid cache
            const auto [_, completionOpt] = previousCacheOpt.value();
            try {
                if (completionOpt.has_value()) {
                    // In cache
                    _cancelCompletion(UserAction::DeleteBackward, false);
                    logger::log("Cancel previous cached completion");
                    _insertCompletion(completionOpt.value().stringify());
                    logger::log("Insert previous cached completion");
                }
                else {
                    // Out of cache
                    _cancelCompletion();
                    logger::log("Canceled by delete backward.");
                }
            }
            catch (runtime_error&e) {
                logger::log(e.what());
            }
        }
    }
    else {
        cancelByModifyLine(_keyHelper.toKeycode(Key::BackSpace));
    }
}

void RegistryMonitor::cancelByKeycodeNavigate(Keycode) {
    _isContinuousEnter.store(false);
    if (_completionCache.valid()) {
        try {
            _cancelCompletion(UserAction::Navigate);
            logger::log("Canceled by navigate.");
        }
        catch (runtime_error&e) {
            logger::log(e.what());
        }
    }
}

void RegistryMonitor::cancelByModifyLine(const Keycode keycode) {
    const auto isEnter = keycode == _keyHelper.toKeycode(Key::Enter);
    _isJustAccepted.store(false);
    if (_completionCache.valid()) {
        try {
            _cancelCompletion(UserAction::ModifyLine);
            logger::log(format("Canceled by {}", isEnter ? "backspace" : "enter"));
        }
        catch (runtime_error&e) {
            logger::log(e.what());
        }
    }

    if (isEnter) {
        if (_isContinuousEnter) {
            logger::log("Detect Continuous enter, retrieve completion directly");
            _retrieveCompletion(_prefix);
        }
        else {
            logger::log("Detect first enter, retrieve editor info first");
            _isContinuousEnter.store(true);
            _retrieveEditorInfo();
        }
    }
    else {
        _isContinuousEnter.store(false);
    }
}

void RegistryMonitor::cancelBySave() {
    if (_completionCache.valid()) {
        _cancelCompletion(UserAction::Navigate);
        logger::log("Canceled by save.");
        WindowInterceptor::GetInstance()->sendSave();
    }
    else {
        _needInsert.store(false);
    }
}

void RegistryMonitor::cancelByUndo() {
    _isContinuousEnter.store(false);
    const auto windowInterceptor = WindowInterceptor::GetInstance();
    if (_isJustAccepted.load()) {
        _isJustAccepted.store(false);
        windowInterceptor->sendUndo();
        windowInterceptor->sendUndo();
    }
    else if (_completionCache.valid()) {
        _completionCache.reset();
        logger::log("Canceled by undo");
        windowInterceptor->sendUndo();
    }
    else {
        _needInsert.store(false);
    }
}

void RegistryMonitor::processNormalKey(Keycode keycode) {
    _isContinuousEnter.store(false);
    _isJustAccepted.store(false);
    if (const auto nextCacheOpt = _completionCache.next(); nextCacheOpt.has_value()) {
        // Has valid cache
        const auto [currentChar, completionOpt] = nextCacheOpt.value();
        try {
            // TODO: Check if this would cause issues
            if (keycode == currentChar) {
                // Cache hit
                if (completionOpt.has_value()) {
                    // In cache
                    _cancelCompletion(UserAction::DeleteBackward, false);
                    logger::log("Canceled due to update cache");
                    _insertCompletion(completionOpt.value().stringify());
                }
                else {
                    // Out of cache
                    logger::log("Accept due to fill in cache");
                    acceptByTab(keycode);
                }
            }
            else {
                // Cache miss
                _cancelCompletion();
                logger::log(format("Canceled due to cache miss (keycode: {})", keycode));
                _retrieveEditorInfo();
            }
        }
        catch (runtime_error&e) {
            logger::log(e.what());
        }
    }
    else {
        // No valid cache
        _retrieveEditorInfo();
    }
}

void RegistryMonitor::_cancelCompletion(const UserAction action, const bool resetCache) {
    system::setEnvironmentVariable("CMWCODER_cancelType", to_string(enum_integer(action)));
    WindowInterceptor::GetInstance()->sendCancelCompletion();
    if (resetCache) {
        _completionCache.reset();
    }
}

void RegistryMonitor::_insertCompletion(const string&data) {
    system::setEnvironmentVariable("CMWCODER_completionGenerated", data);
    WindowInterceptor::GetInstance()->sendInsertCompletion();
}

void RegistryMonitor::_reactToCompletion(Completion&&completion, const bool isAccept) {
    const auto path = isAccept ? "/completion/accept" : "/completion/insert";
    try {
        auto client = httplib::Client("http://localhost:3000");
        client.set_connection_timeout(3);
        if (auto res = client.Post(
            path,
            nlohmann::json{
                {"completion", encode(completion.stringify(), crypto::Encoding::Base64)},
                {"projectId", _projectId},
                {"version", _pluginVersion},
            }.dump(),
            "application/json"
        )) {
            if (res->status == 200) {
                const auto responseBody = nlohmann::json::parse(res->body);
                logger::log(format("({}) Result: {}", path, responseBody["result"].get<string>()));
            }
            else {
                logger::log(
                    format("({}) Request failed, status: {}, body: {}", path, res->status, res->body)
                );
            }
        }
        else {
            logger::error(format("({}) Http error: {}", path, httplib::to_string(res.error())));
        }
    }
    catch (exception&e) {
        logger::error(format("({}) Exception: {}", path, e.what()));
    }
}

void RegistryMonitor::_retrieveCompletion(const string&prefix) {
    _lastTriggerTime = chrono::high_resolution_clock::now();
    thread([this, prefix, currentTriggerName = _lastTriggerTime.load()] {
        _needInsert.store(true);
        optional<string> completionGenerated;
        try {
            auto client = httplib::Client("http://localhost:3000");
            client.set_connection_timeout(10);
            client.set_read_timeout(10);
            client.set_write_timeout(10);
            if (auto res = client.Post(
                "/completion/generate",
                nlohmann::json{
                    {"cursorString", _cursorString},
                    {"path", encode(_path, crypto::Encoding::Base64)},
                    {"prefix", encode(prefix, crypto::Encoding::Base64)},
                    {"projectId", _projectId},
                    {"suffix", encode(_suffix, crypto::Encoding::Base64)},
                    {"symbolString", encode(_symbolString, crypto::Encoding::Base64)},
                    {"tabString", encode(_tabString, crypto::Encoding::Base64)},
                    {"version", _pluginVersion},
                }.dump(),
                "application/json"
            )) {
                if (res->status == 200) {
                    const auto responseBody = nlohmann::json::parse(res->body);
                    const auto result = responseBody["result"].get<string>();
                    if (const auto&contents = responseBody["contents"];
                        result == "success" && contents.is_array() && !contents.empty()) {
                        completionGenerated.emplace(decode(contents[0].get<string>(), crypto::Encoding::Base64));
                        logger::log(format("(/completion/generate) Completion: {}",
                                           completionGenerated.value_or("null")));
                    }
                    else {
                        logger::log(format("(/completion/generate) Completion is invalid: {}", result));
                    }
                }
                else {
                    logger::log(
                        format("(/completion/generate) Request failed, status: {}, body: {}", res->status, res->body)
                    );
                }
            }
            else {
                logger::log(format("(/completion/generate) HTTP error: {}", httplib::to_string(res.error())));
            }
        }
        catch (exception&e) {
            logger::log(format("(/completion/generate) Exception: {}", e.what()));
        }
        catch (...) {
            logger::log("(/completion/generate) Unknown exception.");
        }
        if (_needInsert.load() && completionGenerated.has_value() && currentTriggerName == _lastTriggerTime.load()) {
            try {
                auto oldCompletion = _completionCache.reset(
                    completionGenerated.value()[0] == '1',
                    completionGenerated.value().substr(1)
                );
                if (!oldCompletion.content().empty()) {
                    _cancelCompletion(UserAction::DeleteBackward, false);
                    logger::log(format("Cancel old cached completion: {}", oldCompletion.stringify()));
                }
                _insertCompletion(completionGenerated.value());
                logger::log("Inserted completion");
                thread(&RegistryMonitor::_reactToCompletion, this, _completionCache.completion(), false).detach();
            }
            catch (runtime_error&e) {
                logger::log(e.what());
            }
        }
    }).detach();
}

void RegistryMonitor::_retrieveEditorInfo() const {
    if (_isAutoCompletion.load()) {
        WindowInterceptor::GetInstance()->requestRetrieveInfo();
    }
}

void RegistryMonitor::_retrieveProjectId(const string&projectFolder) {
    if (const auto currentProjectHash = crypto::sha1(projectFolder); _projectHash != currentProjectHash) {
        _projectId.clear();
        _projectHash = currentProjectHash;
    }

    if (_projectId.empty()) {
        const auto projectListKey = _subKey + "\\Project List";
        while (_projectId.empty()) {
            if (const auto projectId = system::getRegValue(projectListKey, _projectHash); projectId.has_value()) {
                _projectId = projectId.value();
            }
            else {
                _projectId = InputBox("Please input current project's iSoft ID", "Input Project ID");
                if (_projectId.empty()) {
                    logger::error("Project ID is empty.");
                }
                else {
                    system::setRegValue(projectListKey, _projectHash, _projectId);
                }
            }
        }
    }
}

void RegistryMonitor::_threadCompletionMode() {
    thread([this] {
        while (_isRunning.load()) {
            if (const auto isAutoCompletionOpt = system::getRegValue(_subKey, "CMWCODER_autoCompletion");
                isAutoCompletionOpt.has_value()) {
                if (const auto isAutoCompletion = static_cast<bool>(stoi(isAutoCompletionOpt.value()));
                    _isAutoCompletion.load() != isAutoCompletion) {
                    _isAutoCompletion.store(isAutoCompletion);
                    logger::log(format("Auto completion: {}", _isAutoCompletion.load() ? "on" : "off"));
                }
            }
            this_thread::sleep_for(chrono::milliseconds(10));
        }
    }).detach();
}

void RegistryMonitor::_threadLogDebug() const {
    thread([this] {
        while (_isRunning.load()) {
            constexpr auto envName = "CMWCODER_logDebug";
            if (const auto debugStringOpt = system::getEnvironmentVariable(envName);
                debugStringOpt.has_value()) {
                logger::log(format("[SI] {}", debugStringOpt.value()));
                system::setEnvironmentVariable(envName);
            }
            this_thread::sleep_for(chrono::milliseconds(1));
        }
    }).detach();
}

void RegistryMonitor::_threadProcessInfo() {
    thread([this] {
        while (_isRunning.load()) {
            if (const auto currentLinePrefixOpt = system::getEnvironmentVariable("CMWCODER_currentPrefix");
                currentLinePrefixOpt.has_value()) {
                system::setEnvironmentVariable("CMWCODER_currentPrefix");
                const auto currentLinePrefix = regex_replace(
                    regex_replace(currentLinePrefixOpt.value(), regex(R"(\\r\\n)"), "\r\n"),
                    regex(R"(\=)"),
                    "="
                );
                if (const auto lastNewLineIndex = _prefix.find_last_of('\n'); lastNewLineIndex != string::npos) {
                    _retrieveCompletion(_prefix.substr(0, lastNewLineIndex + 1) + currentLinePrefix);
                }
                else {
                    _retrieveCompletion(currentLinePrefix);
                }
            }
            else {
                optional<string> suffixOpt, prefixOpt;
                if (Configurator::GetInstance()->version().first == SiVersion::Major::V35) {
                    suffixOpt = system::getEnvironmentVariable("CMWCODER_suffix");
                    prefixOpt = system::getEnvironmentVariable("CMWCODER_prefix");
                }
                else {
                    suffixOpt = system::getRegValue(_subKey, "CMWCODER_suffix");
                    prefixOpt = system::getRegValue(_subKey, "CMWCODER_prefix");
                }
                const auto projectOpt = system::getEnvironmentVariable("CMWCODER_project");
                const auto pathOpt = system::getEnvironmentVariable("CMWCODER_path");
                if (
                    const auto cursorStringOpt = system::getEnvironmentVariable("CMWCODER_cursor");
                    suffixOpt.has_value() &&
                    prefixOpt.has_value() &&
                    projectOpt.has_value() &&
                    pathOpt.has_value() &&
                    cursorStringOpt.has_value()
                ) {
                    if (_pluginVersion.empty()) {
                        if (const auto versionOpt = system::getEnvironmentVariable("CMWCODER_version");
                            versionOpt.has_value()) {
                            _pluginVersion = Configurator::GetInstance()->reportVersion(versionOpt.value());
                            logger::log(format("Plugin version: {}", _pluginVersion));
                        }
                        else {
                            continue;
                        }
                    }
                    _retrieveProjectId(regex_replace(projectOpt.value(), regex(R"(\\\\)"), "/"));

                    _symbolString = system::getEnvironmentVariable("CMWCODER_symbols").value_or("");
                    _tabString = system::getEnvironmentVariable("CMWCODER_tabs").value_or("");

                    if (Configurator::GetInstance()->version().first == SiVersion::Major::V35) {
                        system::setEnvironmentVariable("CMWCODER_suffix");
                        system::setEnvironmentVariable("CMWCODER_prefix");
                    }
                    else {
                        system::deleteRegValue(_subKey, "CMWCODER_suffix");
                        system::deleteRegValue(_subKey, "CMWCODER_prefix");
                    }
                    system::setEnvironmentVariable("CMWCODER_symbols");
                    system::setEnvironmentVariable("CMWCODER_version");
                    system::setEnvironmentVariable("CMWCODER_tabs");
                    system::setEnvironmentVariable("CMWCODER_project");
                    system::setEnvironmentVariable("CMWCODER_path");
                    system::setEnvironmentVariable("CMWCODER_cursor");
                    _suffix = regex_replace(
                        regex_replace(suffixOpt.value(), regex(R"(\\r\\n)"), "\r\n"),
                        regex(R"(\=)"),
                        "="
                    );
                    _path = regex_replace(pathOpt.value(), regex(R"(\\\\)"), "/");
                    _prefix = regex_replace(
                        regex_replace(prefixOpt.value(), regex(R"(\\r\\n)"), "\r\n"),
                        regex(R"(\=)"),
                        "="
                    );
                    _cursorString = cursorStringOpt.value();

                    _retrieveCompletion(_prefix);
                }
            }
            this_thread::sleep_for(chrono::milliseconds(1));
        }
    }).detach();
}
