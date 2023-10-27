#pragma once

#include <chrono>
#include <string>

#include <nlohmann/json.hpp>

#include <types/Completion.h>

namespace types {
    class Statistics {
    public:
        Statistics(Completion completion, std::string projectId);

        [[nodiscard]] nlohmann::json parse();

    private:
        int64_t _beginEpochSeconds, _endEpochSeconds;
        const Completion _completion;
        const std::string _projectId;
    };
}