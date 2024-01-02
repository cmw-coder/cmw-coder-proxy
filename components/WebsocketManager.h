#pragma once

#include <chrono>

#include <ixwebsocket/IXWebSocket.h>
#include <nlohmann/json.hpp>
#include <singleton_dclp.hpp>

#include <types/WsAction.h>

namespace components {
    class WebsocketManager : public SingletonDclp<WebsocketManager> {
    public:
        using CallBack = std::function<void(const nlohmann::json&)>;

        explicit WebsocketManager(
            std::string&& url,
            const std::chrono::seconds& pingInterval = std::chrono::seconds{30}
        );

        ~WebsocketManager() override;

        template<class T>
        void registerAction(
            const types::WsAction action,
            T* const other,
            void (T::* const memberFunction)(const nlohmann::json&)
        ) {
            _handlerMap[action].push_back(std::bind_front(memberFunction, other));
        }

        void sendAction(types::WsAction action);

        void sendAction(types::WsAction action, nlohmann::json&& data);

        void sendRaw(const std::string& message);

    private:
        ix::WebSocket _client;
        std::unordered_map<types::WsAction, std::vector<CallBack>> _handlerMap;

        void _handleEventMessage(const std::string& messageString);
    };
}
