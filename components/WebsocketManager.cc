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
