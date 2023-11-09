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
    const regex editorInfoRegex(
            R"regex(^cursor="(.*?)";path="(.*?)";project="(.*?)";tabs="(.*?)";type="(.*?)";version="(.*?)";symbols="(.*?)";prefix="(.*?)";suffix="(.*?)"$)regex");
//    const regex cursorRegex(
//            R"regex(^lnFirst="(.*?)";ichFirst="(.*?)";lnLast="(.*?)";ichLim="(.*?)";fExtended="(.*?)";fRect="(.*?)"$)regex");
}

RegistryMonitor::RegistryMonitor() :
        _subKey(Configurator::GetInstance()->version().first == SiVersion::Major::V35
                ? R"(SOFTWARE\Source Dynamics\Source Insight\3.0)"
                : R"(SOFTWARE\Source Dynamics\Source Insight\4.0)") {
    thread([this] {
        while (_isRunning.load()) {
            try {
                const auto editorInfoString = system::getRegValue(_subKey, "editorInfo");
//                logger::log(format("editorInfoString: {}", editorInfoString));
                system::deleteRegValue(_subKey, "editorInfo");

                smatch editorInfoRegexResults;
                if (!regex_match(editorInfoString, editorInfoRegexResults, editorInfoRegex) ||
                    editorInfoRegexResults.size() != 10) {
                    logger::log(
                            format("Invalid editorInfoString, original string: {}, result size: {}", editorInfoString,
                                   editorInfoRegexResults.size()));
                    continue;
                }

                const auto projectFolder = regex_replace(editorInfoRegexResults[3].str(), regex(R"(\\\\)"), "/");

                _retrieveProjectId(projectFolder);

                _retrieveCompletion(editorInfoString);

                const auto version = editorInfoRegexResults[5].str();
                if (!version.empty() && _pluginVersion.empty()) {
                    _pluginVersion = Configurator::GetInstance()->reportVersion(version);
                    logger::log(format("Plugin version: {}", _pluginVersion));
                }

                // TODO: Finish completionType
                /*nlohmann::json editorInfo = {
                        {"cursor",          nlohmann::json::object()},
                        {"currentFilePath", regex_replace(editorInfoRegexResults[2].str(), regex(R"(\\\\)"), "/")},
                        {"projectFolder",   regex_replace(editorInfoRegexResults[3].str(), regex(R"(\\\\)"), "/")},
                        {"openedTabs",      nlohmann::json::array()},
                        {"completionType",  stoi(editorInfoRegexResults[5].str()) > 0 ? "snippet" : "line"},
                        {"version",         editorInfoRegexResults[6].str()},
                        {"symbols",         nlohmann::json::array()},
                        {"prefix",          editorInfoRegexResults[8].str()},
                        {"suffix",          editorInfoRegexResults[9].str()},
                };

                {
                    const auto cursorString = regex_replace(editorInfoRegexResults[1].str(), regex(R"(\\)"), "");
                    smatch cursorRegexResults;
                    if (!regex_match(cursorString, cursorRegexResults, cursorRegex) ||
                        cursorRegexResults.size() != 7) {
                        logger::log("Invalid cursorString");
                        continue;
                    }
                    editorInfo["cursor"] = {
                            {"startLine",      cursorRegexResults[1].str()},
                            {"startCharacter", cursorRegexResults[2].str()},
                            {"endLine",        cursorRegexResults[3].str()},
                            {"endCharacter",   cursorRegexResults[4].str()},
                    };
                }

                {
                    const auto symbolString = editorInfoRegexResults[7].str();
                    editorInfo["symbols"] = nlohmann::json::array();
                    if (symbolString.length() > 2) {
                        for (const auto symbol: views::split(symbolString.substr(1, symbolString.length() - 1), "||")) {
                            const auto symbolComponents = views::split(symbol, "|")
                                                          | views::transform(
                                    [](auto &&rng) { return string(&*rng.begin(), ranges::distance(rng)); })
                                                          | to<vector>();

                            editorInfo["symbols"].push_back(
                                    {
                                            {"name",      symbolComponents[0]},
                                            {"path",      symbolComponents[1]},
                                            {"startLine", symbolComponents[2]},
                                            {"endLine",   symbolComponents[3]},
                                    }
                            );
                        }
                    }
                }

                {
                    const string tabsString = editorInfoRegexResults[4].str();
                    auto searchStart(tabsString.cbegin());
                    smatch tabsRegexResults;
                    while (regex_search(
                            searchStart,
                            tabsString.cend(),
                            tabsRegexResults,
                            regex(R"regex(.*?\.([ch]))regex")
                    )) {
                        editorInfo["openedTabs"].push_back(tabsRegexResults[0].str());
                        searchStart = tabsRegexResults.suffix().first;
                    }
                }

                logger::log(editorInfo.dump());*/
            } catch (runtime_error &e) {
            } catch (exception &e) {
                logger::log(e.what());
            } catch (...) {
                logger::log("Unknown exception.");
            }
            this_thread::sleep_for(chrono::milliseconds(1));
        }
    }).detach();
    _threadLogDebug();
    _threadDebounceRetrieveInfo();
}

RegistryMonitor::~RegistryMonitor() {
    _isRunning.store(false);
}

void RegistryMonitor::acceptByTab(Keycode) {
    _justInserted = true;
    auto completion = _completionCache.reset();
    if (!completion.content().empty()) {
        WindowInterceptor::GetInstance()->sendAcceptCompletion();
        logger::log(format("Accepted completion: {}", completion.stringify()));
        thread(&RegistryMonitor::_reactToCompletion, this, std::move(completion)).detach();
    }
}

void RegistryMonitor::cancelByCursorNavigate(CursorPosition, CursorPosition) {
    cancelByKeycodeNavigate(-1);
}

void RegistryMonitor::cancelByDeleteBackward(CursorPosition oldPosition, CursorPosition newPosition) {
    if (oldPosition.line == newPosition.line) {
        const auto previousCacheOpt = _completionCache.previous();
        if (previousCacheOpt.has_value()) {
            // Has valid cache
            const auto [_, completionOpt] = previousCacheOpt.value();
            try {
                if (completionOpt.has_value()) {
                    // In cache
                    _cancelCompletion(UserAction::DeleteBackward, false);
                    _insertCompletion(completionOpt.value().stringify());
                    logger::log("Insert previous cached completion");
                } else {
                    // Out of cache
                    _cancelCompletion();
                    logger::log("Canceled by delete backward.");
                }
            } catch (runtime_error &e) {
                logger::log(e.what());
            }
        }
    } else {
        cancelByModifyLine(enum_integer(Key::BackSpace));
    }
}

void RegistryMonitor::cancelByKeycodeNavigate(Keycode) {
    if (_completionCache.valid()) {
        try {
            _cancelCompletion(UserAction::Navigate);
            logger::log("Canceled by navigate.");
        } catch (runtime_error &e) {
            logger::log(e.what());
        }
    }
}

void RegistryMonitor::cancelByModifyLine(Keycode keycode) {
    const auto windowInterceptor = WindowInterceptor::GetInstance();
    _justInserted = false;
    if (_completionCache.valid()) {
        try {
            _cancelCompletion(UserAction::ModifyLine);
            logger::log(format("Canceled by {}", keycode == enum_integer(Key::BackSpace) ? "backspace" : "enter"));
        } catch (runtime_error &e) {
            logger::log(e.what());
        }
    }

    if (keycode != enum_integer(Key::BackSpace)) {
        windowInterceptor->sendRetrieveInfo();
    }
}

void RegistryMonitor::cancelBySave() {
    if (_completionCache.valid()) {
        _cancelCompletion(UserAction::Navigate);
        logger::log("Canceled by save.");
        WindowInterceptor::GetInstance()->sendSave();
    }
}

void RegistryMonitor::cancelByUndo() {
    const auto windowInterceptor = WindowInterceptor::GetInstance();
    if (_justInserted.load()) {
        _justInserted = false;
        windowInterceptor->sendUndo();
        windowInterceptor->sendUndo();
    } else if (_completionCache.valid()) {
        _completionCache.reset();
        logger::log(("Canceled by undo"));
        windowInterceptor->sendUndo();
    }
}

void RegistryMonitor::processNormalKey(Keycode keycode) {
    _justInserted = false;

    const auto nextCacheOpt = _completionCache.next();
    if (nextCacheOpt.has_value()) {
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
                } else {
                    // Out of cache
                    logger::log("Accept due to fill in cache");
                    acceptByTab(keycode);
                }
            } else {
                // Cache miss
                _cancelCompletion();
                logger::log(format("Canceled due to cache miss (keycode: {})", keycode));
                _requestEditorInfo();
            }
        } catch (runtime_error &e) {
            logger::log(e.what());
        }
    } else {
        // No valid cache
        _requestEditorInfo();
        return;
    }
}

void RegistryMonitor::_cancelCompletion(UserAction action, bool resetCache) {
    system::setRegValue(_subKey, "cancelType", to_string(enum_integer(action)));
    WindowInterceptor::GetInstance()->sendCancelCompletion();
    if (resetCache) {
        _completionCache.reset();
    }
}

void RegistryMonitor::_insertCompletion(const string &data) {
    system::setRegValue(_subKey, "completionGenerated", data);
    WindowInterceptor::GetInstance()->sendInsertCompletion();
}

void RegistryMonitor::_reactToCompletion(Completion &&completion) {
    try {
        auto client = httplib::Client("http://localhost:3000");
        client.set_connection_timeout(3);
        if (auto res = client.Post(
                "/completion/accept",
                nlohmann::json{
                        {"completion", completion.stringify()},
                        {"projectId",  _projectId},
                        {"version",    _pluginVersion},
                }.dump(),
                "application/json"
        )) {
            const auto responseBody = nlohmann::json::parse(res->body);
            logger::log(format("(/completion/accept) Result: {}", responseBody["result"].get<string>()));
        } else {
            logger::log(format("(/completion/accept) Http error: {}", httplib::to_string(res.error())));
        }
    } catch (exception &e) {
        logger::log(format("(/completion/accept) Exception: {}", e.what()));
    }
}

void RegistryMonitor::_requestEditorInfo() {
    if (_isAutoCompletion) {
        _debounceTime.store(chrono::high_resolution_clock::now() + chrono::milliseconds(250));
        _needRetrieveInfo.store(true);
    }
}

void RegistryMonitor::_retrieveCompletion(const string &editorInfoString) {
    _lastTriggerTime = chrono::high_resolution_clock::now();
    thread([this, editorInfoString, currentTriggerName = _lastTriggerTime.load()] {
        optional<string> completionGenerated;
        try {
            auto client = httplib::Client("http://localhost:3000");
            client.set_connection_timeout(10);
            client.set_read_timeout(10);
            client.set_write_timeout(10);
            if (auto res = client.Post(
                    "/completion/generate",
                    nlohmann::json{
                            {"info",      crypto::encode(editorInfoString, crypto::Encoding::Base64)},
                            {"projectId", _projectId},
                            {"version",   _pluginVersion},
                    }.dump(),
                    "application/json"
            )) {
                const auto responseBody = nlohmann::json::parse(res->body);
                const auto result = responseBody["result"].get<string>();
                const auto &contents = responseBody["contents"];
                if (result == "success" && contents.is_array() && !contents.empty()) {
                    completionGenerated.emplace(crypto::decode(contents[0].get<string>(), crypto::Encoding::Base64));
                    logger::log(format("(/completion/generate) Completion: {}", completionGenerated.value_or("null")));
                } else {
                    logger::log(format("(/completion/generate) Completion is invalid: {}", result));
                }
            } else {
                logger::log(format("(/completion/generate) HTTP error: {}", httplib::to_string(res.error())));
            }
        } catch (exception &e) {
            logger::log(format("(/completion/generate) Exception: {}", e.what()));
        }
        if (completionGenerated.has_value() && currentTriggerName == _lastTriggerTime.load()) {
            try {
                _completionCache.reset(completionGenerated.value()[0] == '1', completionGenerated.value().substr(1));
                _insertCompletion(completionGenerated.value());
                logger::log("Inserted completion");
            } catch (runtime_error &e) {
                logger::log(e.what());
            }
        }
    }).detach();
}

void RegistryMonitor::_retrieveProjectId(const string &projectFolder) {
    const auto currentProjectHash = crypto::sha1(projectFolder);
    if (_projectHash != currentProjectHash) {
        _projectId.clear();
        _projectHash = currentProjectHash;
    }

    if (_projectId.empty()) {
        const auto projectListKey = _subKey + "\\Project List";
        while (_projectId.empty()) {
            try {
                _projectId = system::getRegValue(projectListKey, _projectHash);
            } catch (...) {
                _projectId = InputBox("Please input current project's iSoft ID", "Input Project ID");
                if (_projectId.empty()) {
                    logger::error("Project ID is empty.");
                } else {
                    system::setRegValue(projectListKey, _projectHash, _projectId);
                }
            }
        }
    }
}

void RegistryMonitor::_threadCompletionMode() {
    thread([this] {
        const auto autoCompletionKey = "autoCompletion";
        while (_isRunning.load()) {
            try {
                const bool isAutoCompletion = stoi(system::getRegValue(_subKey, autoCompletionKey));
                if (_isAutoCompletion != isAutoCompletion) {
                    _isAutoCompletion = isAutoCompletion;
                    logger::log(format("Auto completion: {}", _isAutoCompletion.load() ? "on" : "off"));
                }
            } catch (runtime_error &e) {
            }
            this_thread::sleep_for(chrono::milliseconds(10));
        }
    }).detach();
}

void RegistryMonitor::_threadDebounceRetrieveInfo() {
    thread([this] {
        while (_isRunning.load()) {
            if (_needRetrieveInfo.load()) {
                const auto deltaTime = _debounceTime.load() - chrono::high_resolution_clock::now();
                if (deltaTime <= chrono::nanoseconds(0)) {
                    WindowInterceptor::GetInstance()->sendRetrieveInfo();
                    _needRetrieveInfo.store(false);
                } else {
                    this_thread::sleep_for(deltaTime);
                }
            } else {
                this_thread::sleep_for(chrono::milliseconds(10));
            }
        }
    }).detach();
}

void RegistryMonitor::_threadLogDebug() {
    thread([this] {
        const auto debugLogKey = "CMWCODER_logDebug";
        while (_isRunning.load()) {
            try {
                const auto logDebugString = system::getRegValue(_subKey, debugLogKey);
                logger::log(format("[SI] {}", logDebugString));
                system::deleteRegValue(_subKey, debugLogKey);
            } catch (runtime_error &e) {
            }
            this_thread::sleep_for(chrono::milliseconds(1));
        }
    }).detach();
}
