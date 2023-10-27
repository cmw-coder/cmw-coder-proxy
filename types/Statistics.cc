#include <utility>

#include <types/Statistics.h>
#include <types/Configurator.h>
#include <utils/logger.h>

using namespace types;
using namespace utils;

Statistics::Statistics(Completion completion, std::string projectId) :
        _completion(std::move(completion)), _projectId(std::move(projectId)) {
    const auto nowTime = std::chrono::system_clock::now().time_since_epoch();
    _beginEpochSeconds = std::chrono::duration_cast<std::chrono::seconds>(nowTime).count();
    _endEpochSeconds = std::chrono::duration_cast<std::chrono::seconds>(nowTime).count();
}

/**
 * Parses the statistics data into a JSON object.
 *
 * @param begin Begin time of the statistics
 * @param count Count of the statistics
 * @param end End time of the statistics
 * @param extra Extra information, usually be plugin version, can be empty
 * @param firstClass First class of the statistics, see {@link FirstClass}
 * @param product Product name, always be "SI"
 * @param secondClass Second class of the statistics, always be "CMW"
 * @param skuName SKU name, always be "ADOPT"
 * @param subType sub-type, always be projectId
 * @param type Type of the statistics, always be "AIGC"
 * @param user User name, the same as domain account name
 * @param userType User type, can be "USER" or "HOST"
 */
nlohmann::json Statistics::parse() {
    auto result = nlohmann::json::array();
    const nlohmann::json basicInfo = {
            {"begin",       _beginEpochSeconds},
            {"end",         _endEpochSeconds},
            {"extra",       Configurator::GetInstance()->pluginVersion()},
            {"product",     "SI"},
            {"secondClass", "CMW"},
            {"subType",     _projectId},
            {"type",        "AIGC"},
            {"user",        Configurator::GetInstance()->username()},
            {"userType",    "USER"},
    };

    const auto completionContent = _completion.content();

    nlohmann::json adoptCharacters = {
            {"count",      completionContent.length()},
            {"firstClass", "ADOPT_CHAR"},
    };
    adoptCharacters.merge_patch(basicInfo);
    result.push_back(adoptCharacters);

    {
        auto lineCount = 1;
        if (_completion.isSnippet()) {
            auto pos = completionContent.find(R"(\n)", 0);
            while (pos != std::string::npos) {
                ++lineCount;
                pos = completionContent.find(R"(\n)", pos + 1);
            }
        }

        nlohmann::json adoptLines = {
                {"count",      lineCount},
                {"firstClass", "CODE"},
                {"skuName",    "ADOPT"},
        };
        adoptLines.merge_patch(basicInfo);
        result.push_back(adoptLines);
    }
    return result;
}