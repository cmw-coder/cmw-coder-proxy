#include <utility>

#include <types/Statistics.h>
#include <types/Configurator.h>
#include <utils/logger.h>

using namespace types;
using namespace utils;

Statistics::Statistics(std::string projectId, std::string completion, bool mode) :
        _projectId(std::move(projectId)),
        user(Configurator::GetInstance()->username()) {
    const auto nowTime = std::chrono::system_clock::now().time_since_epoch();
    _beginEpochSeconds = std::chrono::duration_cast<std::chrono::seconds>(nowTime).count();
    _endEpochSeconds = std::chrono::duration_cast<std::chrono::seconds>(nowTime).count();

    //mode == true时 此时是CharData
    if (mode) {
        firstClass = "ADOPT_CHAR";
        skuName = "";
        const auto isSnippet = completion[0] == '1';
        if (isSnippet) {
            count = completion.length() - 1;
        } else {
            const auto index = completion.find(R"(\n)", 0);
            logger::log(std::format("index: {}", index));
            if (index > 1) {
                count = index - 1;
            } else {
                count = completion.length() - 1;
            }
        }
    } else {
        const auto isSnippet = completion[0] == '1';
        auto lines = 1;
        if (isSnippet) {
            auto pos = completion.find(R"(\n)", 0);
            while (pos != std::string::npos) {
                ++lines;
                pos = completion.find(R"(\n)", pos + 1);
            }
        }
        firstClass = "CODE";
        skuName = "ADOPT";
        count = lines;
    }
}

nlohmann::json Statistics::parse() {
    return {
            {"begin",       _beginEpochSeconds},
            {"end",         _endEpochSeconds},
            {"user",        user},
            {"userType",    "USER"},
            {"count",       count},
            {"extra",       extra},
            {"type",        "AIGC"},
            {"product",     "Source Insight 3"},
            {"firstClass",  firstClass},
            {"secondClass", "CMW"},
            {"skuName",     skuName},
            {"subType",     _projectId},
    };
}