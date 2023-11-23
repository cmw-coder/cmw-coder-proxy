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

RegistryMonitor::RegistryMonitor() : _subKey(Configurator::GetInstance()->version().first == SiVersion::Major::V35
                                                 ? R"(SOFTWARE\Source Dynamics\Source Insight\3.0)"
                                                 : R"(SOFTWARE\Source Dynamics\Source Insight\4.0)") {
    try {
        _isAutoCompletion.store(stoi(system::getRegValue(_subKey, "autoCompletion")));
    }
    catch (...) {
    }

    logger::log(format("Auto completion: {}", _isAutoCompletion.load() ? "on" : "off"));

    _threadProcessInfo();
    _threadProcessInfo();
    _threadCompletionMode();
    _threadLogDebug();
}

RegistryMonitor::~RegistryMonitor() {
    _isRunning.store(false);
}

void RegistryMonitor::acceptByTab(Keycode) {
    _justInserted = true;
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
        cancelByModifyLine(enum_integer(Key::BackSpace));
    }
}

void RegistryMonitor::cancelByKeycodeNavigate(Keycode) {
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
    _justInserted = false;
    if (_completionCache.valid()) {
        try {
            _cancelCompletion(UserAction::ModifyLine);
            logger::log(format("Canceled by {}", keycode == enum_integer(Key::BackSpace) ? "backspace" : "enter"));
        }
        catch (runtime_error&e) {
            logger::log(e.what());
        }
    }

    if (keycode != enum_integer(Key::BackSpace)) {
        _retrieveEditorInfo();
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
    const auto windowInterceptor = WindowInterceptor::GetInstance();
    if (_justInserted.load()) {
        _justInserted = false;
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
    _justInserted = false;

    if (const auto nextCacheOpt = _completionCache.next(); nextCacheOpt.has_value()) {
        // Has valid cache
        const auto [currentChar, completionOpt] = nextCacheOpt.value();
        try {
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
    system::setRegValue(_subKey, "cancelType", to_string(enum_integer(action)));
    WindowInterceptor::GetInstance()->sendCancelCompletion();
    if (resetCache) {
        _completionCache.reset();
    }
}

void RegistryMonitor::_insertCompletion(const string&data) const {
    system::setRegValue(_subKey, "completionGenerated", data);
    WindowInterceptor::GetInstance()->sendInsertCompletion();
}

void RegistryMonitor::_reactToCompletion(Completion&&completion, const bool isAccept) {
    try {
        auto client = httplib::Client("http://localhost:3000");
        client.set_connection_timeout(3);
        if (auto res = client.Post(
            isAccept ? "/completion/accept" : "/completion/insert",
            nlohmann::json{
                {"completion", encode(completion.stringify(), crypto::Encoding::Base64)},
                {"projectId", _projectId},
                {"version", _pluginVersion},
            }.dump(),
            "application/json"
        )) {
            const auto responseBody = nlohmann::json::parse(res->body);
            logger::log(format("(/completion/accept) Result: {}", responseBody["result"].get<string>()));
        }
        else {
            logger::error(format("(/completion/accept) Http error: {}", httplib::to_string(res.error())));
        }
    }
    catch (exception&e) {
        logger::error(format("(/completion/accept) Exception: {}", e.what()));
    }
}

void RegistryMonitor::_retrieveCompletion() {
    _lastTriggerTime = chrono::high_resolution_clock::now();
    thread([this, currentTriggerName = _lastTriggerTime.load()] {
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
                    {"prefix", encode(_prefix, crypto::Encoding::Base64)},
                    {"projectId", _projectId},
                    {"suffix", encode(_suffix, crypto::Encoding::Base64)},
                    {"symbolString", encode(_symbolString, crypto::Encoding::Base64)},
                    {"tabString", encode(_tabString, crypto::Encoding::Base64)},
                    {"version", _pluginVersion},
                }.dump(),
                "application/json"
            )) {
                const auto responseBody = nlohmann::json::parse(res->body);
                const auto result = responseBody["result"].get<string>();
                if (const auto&contents = responseBody["contents"];
                    result == "success" && contents.is_array() && !contents.empty()) {
                    completionGenerated.emplace(decode(contents[0].get<string>(), crypto::Encoding::Base64));
                    logger::log(format("(/completion/generate) Completion: {}", completionGenerated.value_or("null")));
                }
                else {
                    logger::log(format("(/completion/generate) Completion is invalid: {}", result));
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
                thread(&RegistryMonitor::_reactToCompletion, this, move(oldCompletion), false).detach();
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
            try {
                _projectId = system::getRegValue(projectListKey, _projectHash);
            }
            catch (...) {
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
            try {
                if (const bool isAutoCompletion = stoi(system::getRegValue(_subKey, "autoCompletion"));
                    _isAutoCompletion.load() != isAutoCompletion) {
                    _isAutoCompletion.store(isAutoCompletion);
                    logger::log(format("Auto completion: {}", _isAutoCompletion.load() ? "on" : "off"));
                }
            }
            catch (runtime_error&) {
            }
            this_thread::sleep_for(chrono::milliseconds(10));
        }
    }).detach();
}

void RegistryMonitor::_threadLogDebug() const {
    thread([this] {
        while (_isRunning.load()) {
            try {
                const auto debugLogKey = "CMWCODER_logDebug";
                const auto logDebugString = system::getRegValue(_subKey, debugLogKey);
                logger::log(format("[SI] {}", logDebugString));
                system::deleteRegValue(_subKey, debugLogKey);
            }
            catch (runtime_error&) {
            }
            this_thread::sleep_for(chrono::milliseconds(1));
        }
    }).detach();
}

void RegistryMonitor::_threadProcessInfo() {
    thread([this] {
        while (_isRunning.load()) {
            try {
                const auto currentLinePrefix = regex_replace(
                    regex_replace(
                        system::getRegValue(_subKey, "CMWCODER_curfix"),
                        regex(R"(\\r\\n)"),
                        "\r\n"
                    ),
                    regex(R"(\=)"),
                    "="
                );
                system::deleteRegValue(_subKey, "CMWCODER_curfix");

                if (const auto lastNewLineIndex = _prefix.find_last_of("\r\n"); lastNewLineIndex != string::npos) {
                    _prefix = _prefix.substr(0, lastNewLineIndex + 2) + currentLinePrefix;
                }
                else {
                    _prefix = currentLinePrefix;
                }
                logger::log(format("New prefix: {}", _prefix));
                _retrieveCompletion();
            }
            catch (runtime_error&) {
                try {
                    _suffix = regex_replace(
                        regex_replace(
                            system::getRegValue(_subKey, "CMWCODER_suffix"),
                            regex(R"(\\r\\n)"),
                            "\r\n"
                        ),
                        regex(R"(\=)"),
                        "="
                    );
                    _prefix = regex_replace(
                        regex_replace(
                            system::getRegValue(_subKey, "CMWCODER_prefix"),
                            regex(R"(\\r\\n)"),
                            "\r\n"
                        ),
                        regex(R"(\=)"),
                        "="
                    );
                    _symbolString = system::getRegValue(_subKey, "CMWCODER_symbols");
                    _tabString = system::getRegValue(_subKey, "CMWCODER_tabs");
                    const auto project = regex_replace(
                        system::getRegValue(_subKey, "CMWCODER_project"),
                        regex(R"(\\\\)"),
                        "/"
                    );
                    _path = regex_replace(
                        system::getRegValue(_subKey, "CMWCODER_path"),
                        regex(R"(\\\\)"),
                        "/"
                    );
                    _cursorString = system::getRegValue(_subKey, "CMWCODER_cursor");

                    system::deleteRegValue(_subKey, "CMWCODER_suffix");
                    system::deleteRegValue(_subKey, "CMWCODER_prefix");
                    system::deleteRegValue(_subKey, "CMWCODER_symbols");
                    system::deleteRegValue(_subKey, "CMWCODER_version");
                    system::deleteRegValue(_subKey, "CMWCODER_tabs");
                    system::deleteRegValue(_subKey, "CMWCODER_project");
                    system::deleteRegValue(_subKey, "CMWCODER_path");
                    system::deleteRegValue(_subKey, "CMWCODER_cursor");

                    if (_pluginVersion.empty()) {
                        _pluginVersion = Configurator::GetInstance()->reportVersion(
                            system::getRegValue(_subKey, "CMWCODER_version")
                        );
                        logger::log(format("Plugin version: {}", _pluginVersion));
                    }

                    _retrieveProjectId(project);

                    _retrieveCompletion();
                }
                catch (runtime_error&) {
                } catch (exception&e) {
                    logger::log(e.what());
                } catch (...) {
                    logger::log("Unknown exception.");
                }
            }
            this_thread::sleep_for(chrono::milliseconds(1));
        }
    }).detach();
}
