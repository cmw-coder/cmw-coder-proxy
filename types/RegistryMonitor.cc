#include <chrono>

#include <json/json.h>

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
}

RegistryMonitor::RegistryMonitor() {
    thread([this] {
        while (this->_isRunning.load()) {
            try {
                auto editorInfo = system::getRegValue(_subKey, "editorInfo");
                _lastTriggerTime = chrono::high_resolution_clock::now();
                system::deleteRegValue(_subKey, "editorInfo");

                thread([this, &editorInfo, currentTriggerName = _lastTriggerTime.load()] {
                    const auto completionGenerated = _generateCompletion(std::move(editorInfo));
                    if (completionGenerated.has_value() && currentTriggerName == _lastTriggerTime.load()) {
                        system::setRegValue(_subKey, "completionGenerated", completionGenerated.value());
                        WindowInterceptor::GetInstance()->sendFunctionKey(VK_F12);
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

void RegistryMonitor::cancelByCursorNavigate(CursorPosition, CursorPosition) {
    cancelByKeycodeNavigate(-1);
}

void RegistryMonitor::cancelByDeleteBackward(CursorPosition oldPosition, CursorPosition newPosition) {
    if (oldPosition.line == newPosition.line) {
        try {
            system::setRegValue(_subKey, "cancelType", to_string(static_cast<int>(UserAction::DeleteBackward)));
            WindowInterceptor::GetInstance()->sendFunctionKey(VK_F9);
            logger::log("Canceled by delete backward.");
        } catch (runtime_error &e) {
            logger::log(e.what());
        }
    } else {
        cancelByModifyLine(-1);
    }
}

void RegistryMonitor::cancelByKeycodeNavigate(unsigned int) {
    try {
        system::setRegValue(_subKey, "cancelType", to_string(static_cast<int>(UserAction::Navigate)));
        WindowInterceptor::GetInstance()->sendFunctionKey(VK_F9);
        logger::log("Canceled by keycode navigate.");
    } catch (runtime_error &e) {
        logger::log(e.what());
    }
}

void RegistryMonitor::cancelByModifyLine(unsigned int) {
    try {
        system::setRegValue(_subKey, "cancelType", to_string(static_cast<int>(UserAction::ModifyLine)));
        WindowInterceptor::GetInstance()->sendFunctionKey(VK_F9);
        logger::log("Canceled by modify line.");
        WindowInterceptor::GetInstance()->sendFunctionKey(VK_F11);
        logger::log("Retrieve editor info.");
    } catch (runtime_error &e) {
        logger::log(e.what());
    }
}

optional<string> RegistryMonitor::_generateCompletion(string &&editorInfo) {
    Json::Value requestBody, responseBody;
    requestBody["info"] = base64::base64Encode(editorInfo);
    auto client = httplib::Client("http://localhost:3000");
    client.set_connection_timeout(5);
    if (auto res = client.Post(
            "/generate",
            stringify(requestBody),
            "application/json"
    )) {
        stringstream(res->body) >> responseBody;
        if (responseBody["result"].asString() == "success") {
            return base64::decode(responseBody["content"].asString());
        }
        logger::log("HTTP result: " + responseBody["result"].asString());
    } else {
        logger::log("HTTP error: " + httplib::to_string(res.error()));
    }
    return nullopt;
}
