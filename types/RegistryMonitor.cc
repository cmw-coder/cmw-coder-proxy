#include <chrono>
//#include <ranges>
#include <regex>

#include <json/json.h>

#include <types/Configurator.h>
#include <types/RegistryMonitor.h>
#include <types/UserAction.h>
#include <types/WindowInterceptor.h>
#include <utils/base64.h>
#include <utils/hashpp.h>
#include <utils/httplib.h>
#include <utils/inputbox.h>
#include <utils/logger.h>
#include <utils/system.h>

#include <windows.h>

using namespace std;
using namespace std::ranges;
using namespace types;
using namespace utils;

namespace {
    const regex editorInfoRegex(
            R"regex(^cursor="(.*?)";path="(.*?)";project="(.*?)";tabs="(.*?)";type="(.*?)";version="(.*?)";symbols="(.*?)";prefix="(.*?)";suffix="(.*?)"$)regex");
    const regex cursorRegex(
            R"regex(^lnFirst="(.*?)";ichFirst="(.*?)";lnLast="(.*?)";ichLim="(.*?)";fExtended="(.*?)";fRect="(.*?)"$)regex");

    string stringify(const Json::Value &json, const string &indentation = "") {
        Json::StreamWriterBuilder writerBuilder;
        writerBuilder.settings_["indentation"] = indentation;
        unique_ptr<Json::StreamWriter> jsonWriter(writerBuilder.newStreamWriter());
        ostringstream oss;
        jsonWriter->write(json, &oss);
        return oss.str();
    }

    optional<string> generateCompletion(const string &editorInfo, const string &projectId) {
        Json::Value requestBody, responseBody;
        requestBody["info"] = base64::to_base64(editorInfo);
        requestBody["projectId"] = projectId;
        auto client = httplib::Client("http://localhost:3000");
        client.set_connection_timeout(5);
        if (auto res = client.Post(
                "/generate",
                stringify(requestBody),
                "application/json"
        )) {
            stringstream(res->body) >> responseBody;
            if (responseBody["result"].asString() == "success" &&
                responseBody["contents"].isArray() &&
                !responseBody["contents"].empty() &&
                !responseBody["contents"][0].asString().empty()) {
                return base64::from_base64(responseBody["contents"][0].asString());
            }
            logger::log("HTTP result: " + responseBody["result"].asString());
        } else {
            logger::log("HTTP error: " + httplib::to_string(res.error()));
        }
        return nullopt;
    }

    void completionReaction(const string &projectId) {
        try {
            Json::Value requestBody, responseBody;
            requestBody["tabOutput"] = true;
            requestBody["text_lenth"] = 1;
            requestBody["username"] = Configurator::GetInstance()->username();
            requestBody["code_line"] = 1;
            requestBody["project_id"] = projectId;
            requestBody["total_lines"] = 1;
            requestBody["version"] = "SI-0.5.2";
            requestBody["mode"] = false;
            auto client = httplib::Client("http://10.113.10.68:4322");
            client.set_connection_timeout(5);
            client.Post("/code/statistical", stringify(requestBody), "application/json");
        } catch (...) {}
    }
}

RegistryMonitor::RegistryMonitor() {
    thread([this] {
        while (this->_isRunning.load()) {
            try {
                const auto editorInfoString = system::getRegValue(_subKey, "editorInfo");

                smatch editorInfoRegexResults;
                if (!regex_match(editorInfoString, editorInfoRegexResults, editorInfoRegex) ||
                    editorInfoRegexResults.size() != 10) {
                    logger::log("Invalid editorInfoString");
                    continue;
                }

                {
                    const auto currentProjectHash = hashpp::get::getHash(
                            hashpp::ALGORITHMS::SHA1,
                            regex_replace(editorInfoRegexResults[3].str(), regex(R"(\\\\)"), "/")
                    );

                    if (_projectHash != currentProjectHash) {
                        _projectId.clear();
                        _projectHash = currentProjectHash;
                    }
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

                Json::Value editorInfo;
                const auto cursorString = regex_replace(editorInfoRegexResults[1].str(), regex(R"(\\)"), "");
                smatch cursorRegexResults;
                if (!regex_match(cursorString, cursorRegexResults, cursorRegex) ||
                    cursorRegexResults.size() != 7) {
                    logger::log("Invalid cursorString");
                    continue;
                }
                editorInfo["cursor"]["startLine"] = cursorRegexResults[1].str();
                editorInfo["cursor"]["startCharacter"] = cursorRegexResults[2].str();
                editorInfo["cursor"]["endLine"] = cursorRegexResults[3].str();
                editorInfo["cursor"]["endCharacter"] = cursorRegexResults[4].str();

                const auto symbolString = editorInfoRegexResults[7].str();
                editorInfo["symbols"] = Json::arrayValue;
                if (symbolString.length() > 2) {
                    for (const auto symbol: views::split(symbolString.substr(1, symbolString.length() - 1), "||")) {
                        Json::Value symbolInfo;
                        const auto symbolComponents = views::split(symbol, "|")
                                                      | views::transform(
                                [](auto &&rng) { return string(&*rng.begin(), ranges::distance(rng)); })
                                                      | to<vector>();
                        symbolInfo["name"] = symbolComponents[0];
                        symbolInfo["path"] = symbolComponents[1];
                        symbolInfo["startLine"] = symbolComponents[2];
                        symbolInfo["endLine"] = symbolComponents[3];

                        editorInfo["symbols"].append(symbolInfo);
                    }
                }

                auto tabsString = editorInfoRegexResults[4].str();
                editorInfo["openedTabs"] = Json::arrayValue;
                /*smatch tabsRegexResults;
                try {
                    while (regex_search(
                            tabsString,
                            tabsRegexResults,
                            regex(R"regex(.*?\.([ch]))regex", regex_constants::extended)
                    )) {
                        editorInfo["openedTabs"].append(tabsRegexResults[0].str());
                    }
                    editorInfo["suffix"] = editorInfoRegexResults[9].str();
                } catch (exception e) {
                    logger::log(e.what());
                }*/
                editorInfo["currentFilePath"] = regex_replace(editorInfoRegexResults[2].str(), regex(R"(\\\\)"), "/");
                editorInfo["projectFolder"] = regex_replace(editorInfoRegexResults[3].str(), regex(R"(\\\\)"), "/");
                editorInfo["completionType"] = stoi(editorInfoRegexResults[5].str()) > 0 ? "snippet" : "line";
                editorInfo["version"] = editorInfoRegexResults[6].str();
                editorInfo["prefix"] = editorInfoRegexResults[8].str();

                logger::log(stringify(editorInfo));

                _lastTriggerTime = chrono::high_resolution_clock::now();
                system::deleteRegValue(_subKey, "editorInfo");
                thread([this, editorInfoString, currentTriggerName = _lastTriggerTime.load()] {
                    const auto completionGenerated = generateCompletion(editorInfoString, _projectId);
                    if (completionGenerated.has_value() && currentTriggerName == _lastTriggerTime.load()) {
                        system::setRegValue(_subKey, "completionGenerated", completionGenerated.value());
                        WindowInterceptor::GetInstance()->sendFunctionKey(VK_F12);
                        _hasCompletion = true;
                    }
                }).detach();
            } catch (runtime_error &e) {
            } catch (exception &e) {
                logger::log(e.what());
            } catch (...) {
                logger::log("Unknown exception.");
            }
            this_thread::sleep_for(chrono::milliseconds(5));
        }
    }).detach();
}

RegistryMonitor::~RegistryMonitor() {
    this->_isRunning.store(false);
}

void RegistryMonitor::acceptByTab(unsigned int) {
    if (_hasCompletion.load()) {
        _hasCompletion = false;
        WindowInterceptor::GetInstance()->sendFunctionKey(VK_F10);
        thread(completionReaction, _projectId).detach();
        logger::log("Accepted completion");
    }
}

void RegistryMonitor::cancelByCursorNavigate(CursorPosition, CursorPosition) {
    cancelByKeycodeNavigate(-1);
}

void RegistryMonitor::cancelByDeleteBackward(CursorPosition oldPosition, CursorPosition newPosition) {
    if (oldPosition.line == newPosition.line) {
        if (!_hasCompletion.load()) {
            return;
        }
        try {
            system::setRegValue(_subKey, "cancelType", to_string(static_cast<int>(UserAction::DeleteBackward)));
            WindowInterceptor::GetInstance()->sendFunctionKey(VK_F9);
            _hasCompletion = false;
            logger::log("Canceled by delete backward.");
        } catch (runtime_error &e) {
            logger::log(e.what());
        }
    } else {
        cancelByModifyLine(-1);
    }
}

void RegistryMonitor::cancelByKeycodeNavigate(unsigned int) {
    if (!_hasCompletion.load()) {
        return;
    }
    try {
        system::setRegValue(_subKey, "cancelType", to_string(static_cast<int>(UserAction::Navigate)));
        WindowInterceptor::GetInstance()->sendFunctionKey(VK_F9);
        _hasCompletion = false;
        logger::log("Canceled by keycode navigate.");
    } catch (runtime_error &e) {
        logger::log(e.what());
    }
}

void RegistryMonitor::cancelByModifyLine(unsigned int) {
    if (_hasCompletion.load()) {
        try {
            system::setRegValue(_subKey, "cancelType", to_string(static_cast<int>(UserAction::ModifyLine)));
            WindowInterceptor::GetInstance()->sendFunctionKey(VK_F9);
            _hasCompletion = false;
            logger::log("Canceled by modify line.");
        } catch (runtime_error &e) {
            logger::log(e.what());
        }
    }

    WindowInterceptor::GetInstance()->sendFunctionKey(VK_F11);
    logger::log("Retrieve editor info.");
}

void RegistryMonitor::cancelByUndo() {
    _hasCompletion = false;
}
