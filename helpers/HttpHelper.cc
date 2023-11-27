#include <helpers/HttpHelper.h>

using namespace helpers;
using namespace std;

HttpHelper::HttpHelper(string&&host, const chrono::microseconds&timeout) : _client(host) {
    const auto seconds = timeout.count() / 1000000;
    const auto microseconds = timeout.count() % 1000000;
    _client.set_connection_timeout(seconds, microseconds);
}

pair<int, nlohmann::json> HttpHelper::post(const string&path, nlohmann::json&&body) {
    if (auto res = _client.Post(path, body.dump(), "application/json")) {
        return {res->status, nlohmann::json::parse(res->body)};
    }
    else {
        throw HttpError(httplib::to_string(res.error()));
    }
}
