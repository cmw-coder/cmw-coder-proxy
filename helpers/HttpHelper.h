#pragma once

#include <chrono>
#include <string>

#include <httplib.h>
#include <nlohmann/json.hpp>

namespace helpers {
    class HttpHelper {
    public:
        class HttpError final : public std::runtime_error {
        public:
            explicit HttpError(const char* message): runtime_error(message) {}

            explicit HttpError(const std::string& message): runtime_error(message) {}
        };

        explicit HttpHelper(std::string&& host, const std::chrono::microseconds& timeout);

        std::pair<int, nlohmann::json> post(const std::string& path, nlohmann::json&& body);

    private:
        httplib::Client _client;
    };
}
