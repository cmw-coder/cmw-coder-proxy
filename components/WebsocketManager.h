#pragma once

#include <chrono>

#include <ixwebsocket/IXWebSocket.h>
#include <nlohmann/json.hpp>
#include <singleton_dclp.hpp>

#include <models/WsMessage.h>

namespace components {
    class WebsocketManager : public SingletonDclp<WebsocketManager> {
    public:
        using CallBack = std::function<void(nlohmann::json&&)>;

        explicit WebsocketManager(
            std::string&& url,
            const std::chrono::seconds& pingInterval = std::chrono::seconds{30}
        );

        ~WebsocketManager() override;

        void registerAction(
            const types::WsAction action,
            std::function<void(nlohmann::json&&)> handleFunction
        ) {
            _handlerMap[action].push_back(std::move(handleFunction));
        }

        template<class T>
        void registerAction(
            const types::WsAction action,
            T* const other,
            void (T::* const memberFunction)(nlohmann::json&&)
        ) {
            _handlerMap[action].push_back(std::bind_front(memberFunction, other));
        }

        void send(const models::WsMessage& message);

    private:
        ix::WebSocket _client;
        std::unordered_map<types::WsAction, std::vector<CallBack>> _handlerMap;

        void _handleEventMessage(const std::string& messageString);
    };
}
