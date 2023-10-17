#include <helpers/HttpHelper.h>

using namespace helpers;
using namespace std;

HttpHelper::HttpHelper(string &&host, const std::chrono::microseconds &timeout): _client(host) {
    const auto seconds = timeout.count() / 1000000;
    const auto microseconds = timeout.count() % 1000000;
    _client.set_connection_timeout(seconds, microseconds);
}

Json::Value HttpHelper::get(const string &path) {
    return Json::Value();
}
