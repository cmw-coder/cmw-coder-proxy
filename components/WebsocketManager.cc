#include <ixwebsocket/IXNetSystem.h>
#include <magic_enum.hpp>

#include <components/Configurator.h>
#include <components/WebsocketManager.h>
#include <utils/logger.h>

using namespace components;
using namespace models;
using namespace ix;
using namespace magic_enum;
using namespace std;
using namespace types;
using namespace utils;

WebsocketManager::WebsocketManager(string&& url, const chrono::seconds& pingInterval) {
    initNetSystem();
    _client.setUrl(url);
    _client.setPingInterval(static_cast<int>(pingInterval.count()));
    _client.setOnMessageCallback([this](const WebSocketMessagePtr& messagePtr) {
        switch (messagePtr->type) {
            case WebSocketMessageType::Message: {
                _handleEventMessage(messagePtr->str);
                break;
            }
            case WebSocketMessageType::Open: {
                logger::info("Websocket connection established");
                send(HandShakeClientMessage(Configurator::GetInstance()->reportVersion()));
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

    logger::info(format("WebsocketManager is initialized with url: {}", url));
}

WebsocketManager::~WebsocketManager() {
    _client.stop();
    uninitNetSystem();
}

void WebsocketManager::send(const WsMessage& message) {
    try {
        _client.send(message.parse());
    } catch (exception &e) {
        logger::error(e.what());
    }
}

void WebsocketManager::_handleEventMessage(const string& messageString) {
    try {
        if (auto message = nlohmann::json::parse(messageString);
            message.contains("action")) {
            if (const auto actionOpt = enum_cast<WsAction>(message["action"].get<string>());
                actionOpt.has_value()) {
                logger::debug(format("Receive websocket action: {}", message["action"].get<string>()));
                for (const auto& handlers: _handlerMap[actionOpt.value()]) {
                    handlers(nlohmann::json(message["data"]));
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
            "\tError: '{}'.\n"
            "\tMessage: '{}'.",
            e.what(),
            messageString
        ));
    }
}
