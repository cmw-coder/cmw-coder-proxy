#pragma once

#include <chrono>
#include <string>

#include <json/json.h>

#include <utils/httplib.h>

namespace helpers {
    class HttpHelper {
    public:
        explicit HttpHelper(std::string &&host, const std::chrono::microseconds &timeout);

        Json::Value get(const std::string &path);
    private:
        httplib::Client _client;
    };
}
