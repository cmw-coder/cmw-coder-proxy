#include <ixwebsocket/IXNetSystem.h>
#include <magic_enum.hpp>

#include <components/WebsocketManager.h>
#include <utils/logger.h>

using namespace components;
using namespace ix;
using namespace magic_enum;
using namespace std;
using namespace types;
using namespace utils;

WebsocketManager::WebsocketManager(string&& url, const chrono::seconds& pingInterval) {
    initNetSystem();
    _client.setUrl(url);
    _client.setPingInterval(static_cast<int>(pingInterval.count()));
    _client.setOnMessageCallback([](const WebSocketMessagePtr& msg) {
        if (msg->type == WebSocketMessageType::Message) {
            logger::debug(msg->str);
        }
    });
    _client.setOnMessageCallback([this](const WebSocketMessagePtr& messagePtr) {
        switch (messagePtr->type) {
            case WebSocketMessageType::Message: {
                _handleEventMessage(messagePtr->str);
                break;
            }
            case WebSocketMessageType::Open: {
                logger::info("Websocket connection established");
                break;
            }
            case WebSocketMessageType::Close: {
                logger::info(format(
                    "Websocket connection closed.\n"
                    "\tReason: {}. Code: {}. ",
                    messagePtr->closeInfo.reason,
                    messagePtr->closeInfo.code
                ));
                break;
            }
            case WebSocketMessageType::Error: {
                logger::info(format(
                    "Websocket connection error.\n"
                    "\tReason: {}. HTTP Status: {}."
                    "\tRetries: {}. Wait time(ms): {}.",
                    messagePtr->errorInfo.reason,
                    messagePtr->errorInfo.http_status,
                    messagePtr->errorInfo.retries,
                    messagePtr->errorInfo.wait_time
                ));
                break;
            }
            case WebSocketMessageType::Ping:
            case WebSocketMessageType::Pong:
            case WebSocketMessageType::Fragment: {
                break;
            }
        }
    });
    _client.start();
}

WebsocketManager::~WebsocketManager() {
    _client.stop();
    uninitNetSystem();
}

void WebsocketManager::sendAction(const WsAction action) {
    sendRaw(nlohmann::json{
        {"action", enum_name(action)}
    }.dump());
}

void WebsocketManager::sendAction(const WsAction action, nlohmann::json&& data) {
    sendRaw(nlohmann::json{
        {"action", enum_name(action)},
        {"data", move(data)}
    }.dump());
}

void WebsocketManager::sendRaw(const string& message) {
    _client.send(message);
}

void WebsocketManager::_handleEventMessage(const string& messageString) {
    try {
        if (auto message = nlohmann::json::parse(messageString);
            message.contains("action")) {
            if (const auto actionOpt = enum_cast<WsAction>(message["action"].get<string>());
                actionOpt.has_value()) {
                for (const auto& handlers: _handlerMap[actionOpt.value()]) {
                    handlers(message["data"]);
                }
            } else {
                logger::info(format("Invalid websocket message action: {}.", message["action"].get<string>()));
            }
        } else {
            logger::info(format("Invalid websocket message structure: {}.", messageString));
        }
    } catch (nlohmann::detail::parse_error& e) {
        logger::error(format(
            "Websocket message is not a valid JSON.\n"
            "\tError: {}. Message: {}.",
            e.what(),
            messageString
        ));
    }
}
