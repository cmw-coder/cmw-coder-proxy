#pragma once

#include <ixwebsocket/IXWebSocket.h>
#include <nlohmann/json.hpp>

#include <types/WsAction.h>

namespace helpers {
    class WsHelper {
    public:
        enum class Action {
            DebugSync
        };

        explicit WsHelper(std::string&& url, const std::chrono::seconds& pingInterval = std::chrono::seconds{30});

        void sendAction(types::WsAction action, nlohmann::json&& data);

        void sendRaw(const std::string& message);

    private:
        ix::WebSocket _client;
    };
}
