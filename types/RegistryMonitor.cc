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
using namespace std::ranges;
using namespace types;
using namespace utils;

namespace {
    const regex editorInfoRegex(
            R"regex(^cursor="(.*?)";path="(.*?)";project="(.*?)";tabs="(.*?)";type="(.*?)";version="(.*?)";symbols="(.*?)";prefix="(.*?)";suffix="(.*?)"$)regex");
//    const regex cursorRegex(
//            R"regex(^lnFirst="(.*?)";ichFirst="(.*?)";lnLast="(.*?)";ichLim="(.*?)";fExtended="(.*?)";fRect="(.*?)"$)regex");

    optional<string> generateCompletion(const string &editorInfo, const string &projectId) {
        nlohmann::json requestBody = {
                {"info",      crypto::encode(editorInfo, crypto::Encoding::Base64)},
                {"projectId", projectId},
        };
        auto client = httplib::Client("http://localhost:3000");
        client.set_connection_timeout(10);
        client.set_read_timeout(10);
        client.set_write_timeout(10);
        if (auto res = client.Post(
                "/generate",
                requestBody.dump(),
                "application/json"
        )) {
            const auto responseBody = nlohmann::json::parse(res->body);
            const auto result = responseBody["result"].get<string>();
            const auto &contents = responseBody["contents"];
            if (result == "success" && contents.is_array() && !contents.empty()) {
                return crypto::decode(contents[0].get<string>(), crypto::Encoding::Base64);
            }
            logger::log(format("HTTP result: {}", result));
        } else {
            logger::log(format("HTTP error: {}", httplib::to_string(res.error())));
        }
        return nullopt;
    }
}

RegistryMonitor::RegistryMonitor() {
    thread([this] {
        while (this->_isRunning.load()) {
            try {
                const auto editorInfoString = system::getRegValue(_subKey, "editorInfo");

//                logger::log(editorInfoString);

                smatch editorInfoRegexResults;
                if (!regex_match(editorInfoString, editorInfoRegexResults, editorInfoRegex) ||
                    editorInfoRegexResults.size() != 10) {
                    logger::log("Invalid editorInfoString");
                    continue;
                }

                const auto projectFolder = regex_replace(editorInfoRegexResults[3].str(), regex(R"(\\\\)"), "/");

                system::deleteRegValue(_subKey, "editorInfo");

                _retrieveProjectId(projectFolder);

                _retrieveCompletion(editorInfoString);

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
    thread([this] {
        const auto debugLogKey = "CMWCODER_logDebug";
        while (this->_isRunning.load()) {
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

RegistryMonitor::~RegistryMonitor() {
    this->_isRunning.store(false);
}

void RegistryMonitor::acceptByTab(unsigned int) {
    _justInserted = true;
    const auto completion = _completionCache.reset();
    logger::log(format("Accepted completion: {}", completion.stringify()));
    if (!completion.content().empty()) {
        WindowInterceptor::GetInstance()->sendAcceptCompletion();
        thread(&RegistryMonitor::_reactToCompletion, this, ::move(completion)).detach();
        logger::log("Accepted completion");
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

void RegistryMonitor::cancelByKeycodeNavigate(unsigned int) {
    if (_completionCache.valid()) {
        try {
            _cancelCompletion(UserAction::Navigate);
            logger::log("Canceled by navigate.");
        } catch (runtime_error &e) {
            logger::log(e.what());
        }
    }
}

void RegistryMonitor::cancelByModifyLine(unsigned int keycode) {
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

void RegistryMonitor::retrieveEditorInfo(unsigned int keycode) {
    const auto windowInterceptor = WindowInterceptor::GetInstance();
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
//                    logger::log(format("Insert next cached completion: {}", completionOpt.value().stringify()));
                } else {
                    // Out of cache
                    logger::log("Accept due to fill in cache");
                    acceptByTab(keycode);
                }
            } else {
                // Cache miss
                _cancelCompletion();
                logger::log(format("Canceled due to cache miss (keycode: {})", keycode));
                windowInterceptor->sendRetrieveInfo();
                logger::log(format("Retrieving editor info... (keycode: {})", keycode));
            }
        } catch (runtime_error &e) {
            logger::log(e.what());
        }
    } else {
        // No valid cache
        windowInterceptor->sendRetrieveInfo();
        logger::log(format("Retrieving editor info... (keycode: {})", keycode));
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

void RegistryMonitor::_reactToCompletion(CompletionCache::Completion &&completion) {
    try {
        nlohmann::json requestBody;
        auto lines = 1;
        if (completion.isSnippet()) {
            const auto &content = completion.content();
            auto pos = content.find(R"(\n)", 0);
            while (pos != string::npos) {
                ++lines;
                pos = content.find(R"(\n)", pos + 1);
            }
        }
        requestBody = {
                {"code_line",   lines},
                {"mode",        completion.isSnippet()},
                {"project_id",  _projectId},
                {"tab_output",  true},
                {"total_lines", lines},
                {"text_length", completion.content().length()},
                {"username",    Configurator::GetInstance()->username()},
                {"version",     "SI-0.6.1"},
        };
        logger::log(requestBody.dump());
        auto client = httplib::Client("http://10.113.10.68:4322");
        client.set_connection_timeout(3);
        client.Post("/code/statistical", requestBody.dump(), "application/json");
    } catch (...) {}
}

void RegistryMonitor::_retrieveCompletion(const string &editorInfoString) {
    _lastTriggerTime = chrono::high_resolution_clock::now();
    thread([this, editorInfoString, currentTriggerName = _lastTriggerTime.load()] {
        const auto completionGenerated = generateCompletion(editorInfoString, _projectId);
        logger::log(format("Generated completion: {}", completionGenerated.value_or("null")));
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
