#include <chrono>

#include <json/json.h>

#include <types/Configurator.h>
#include <types/RegistryMonitor.h>
#include <types/UserAction.h>
#include <types/WindowInterceptor.h>
#include <utils/base64.h>
#include <utils/httplib.h>
#include <utils/logger.h>
#include <utils/system.h>

#include <windows.h>

using namespace std;
using namespace types;
using namespace utils;

namespace {
    string stringify(const Json::Value &json, const string &indentation = "") {
        Json::StreamWriterBuilder writerBuilder;
        writerBuilder.settings_["indentation"] = indentation;
        std::unique_ptr<Json::StreamWriter> jsonWriter(writerBuilder.newStreamWriter());
        std::ostringstream oss;
        jsonWriter->write(json, &oss);
        return oss.str();
    }

    optional<string> generateCompletion(const string &editorInfo) {
        Json::Value requestBody, responseBody;
        requestBody["info"] = base64::to_base64(editorInfo);
        auto client = httplib::Client("http://localhost:3000");
        client.set_connection_timeout(5);
        if (auto res = client.Post(
                "/generate",
                stringify(requestBody),
                "application/json"
        )) {
            stringstream(res->body) >> responseBody;
            if (responseBody["result"].asString() == "success") {
                return base64::from_base64(responseBody["content"].asString());
            }
            logger::log("HTTP result: " + responseBody["result"].asString());
        } else {
            logger::log("HTTP error: " + httplib::to_string(res.error()));
        }
        return nullopt;
    }

    void completionReaction() {
        try {
            Json::Value requestBody, responseBody;
            requestBody["tabOutput"] = true;
            requestBody["text_lenth"] = 1;
            requestBody["username"] = Configurator::GetInstance()->username();
            requestBody["code_line"] = 1;
            requestBody["total_lines"] = 1;
            requestBody["version"] = "SI-0.5.1";
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
                const auto editorInfo = system::getRegValue(_subKey, "editorInfo");
                _lastTriggerTime = chrono::high_resolution_clock::now();
                system::deleteRegValue(_subKey, "editorInfo");

                thread([this, editorInfo, currentTriggerName = _lastTriggerTime.load()] {
                    const auto completionGenerated = generateCompletion(editorInfo);
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
        thread(completionReaction).detach();
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
