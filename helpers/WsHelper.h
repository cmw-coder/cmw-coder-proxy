#pragma once

#include <ixwebsocket/IXWebSocket.h>

namespace helpers {
    class WsHelper {
    public:
        explicit WsHelper(std::string&& url, const std::chrono::seconds& pingInterval = std::chrono::seconds{30});

        void send(const std::string& message);

    private:
        ix::WebSocket _client;
    };
}
