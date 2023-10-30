#pragma once

#include <chrono>
#include <string>

#include <nlohmann/json.hpp>

#include <types/common.h>
#include <types/Completion.h>

namespace types {
    class Statistics {
    public:
        Statistics(Completion completion, ModelType modelType, std::string pluginVersion, std::string projectId);

        [[nodiscard]] nlohmann::json parse();

    private:
        int64_t _beginEpochSeconds, _endEpochSeconds;
        const Completion _completion;
        const ModelType _modelType;
        const std::string _pluginVersion, _projectId;
    };
}