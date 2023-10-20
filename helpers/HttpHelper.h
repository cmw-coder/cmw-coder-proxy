#pragma once

#include <chrono>
#include <string>
#include <variant>

#include <httplib.h>
#include <nlohmann/json.hpp>

namespace helpers {
    class HttpHelper {
    public:
        explicit HttpHelper(std::string &&host, const std::chrono::microseconds &timeout);

        std::variant<nlohmann::json, std::string> post(const std::string &path, nlohmann::json &&body);

    private:
        httplib::Client _client;
    };
}
