#include <helpers/WsHelper.h>
#include <utils/logger.h>

using namespace helpers;
using namespace ix;
using namespace std;
using namespace utils;

WsHelper::WsHelper(std::string&& url, const std::chrono::seconds& pingInterval) {
    _client.setUrl(url);
    _client.setPingInterval(static_cast<int>(pingInterval.count()));
    _client.setOnMessageCallback([](const WebSocketMessagePtr& msg) {
        if (msg->type == WebSocketMessageType::Message) {
            logger::debug(msg->str);
        }
    });
    _client.start();
}

void WsHelper::send(const std::string& message) {
    _client.send(message);
}
