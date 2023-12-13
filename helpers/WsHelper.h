#pragma once

#include <ixwebsocket/IXWebSocket.h>
#include <nlohmann/json.hpp>

namespace helpers {
    class WsHelper {
    public:
        enum class Action {
            Sync
        };

        explicit WsHelper(std::string&& url, const std::chrono::seconds& pingInterval = std::chrono::seconds{30});

        void sendAction(Action action, nlohmann::json&& data);

        void sendRaw(const std::string& message);

    private:
        ix::WebSocket _client;
    };
}
