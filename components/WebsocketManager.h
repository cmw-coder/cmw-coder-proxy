#pragma once

#include <chrono>

#include <ixwebsocket/IXWebSocket.h>
#include <nlohmann/json.hpp>
#include <singleton_dclp.hpp>

#include <types/WsAction.h>

namespace components {
    class WebsocketManager : public SingletonDclp<WebsocketManager> {
    public:
        explicit WebsocketManager(
            std::string&& url,
            const std::chrono::seconds& pingInterval = std::chrono::seconds{30}
        );

        ~WebsocketManager() override;

        void sendAction(types::WsAction action);

        void sendAction(types::WsAction action, nlohmann::json&& data);

        void sendRaw(const std::string& message);

    private:
        ix::WebSocket _client;
    };
}
