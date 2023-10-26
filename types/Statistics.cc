#include <ctime>

#include <types/Statistics.h>
#include <types/Configurator.h>
#include <utils/logger.h>

using namespace types;
using namespace utils;

SKU::SKU(std::string user, std::string projectId, std::string completion, bool mode):begin(time(0)), end(time(0)), subType(projectId), 
user(Configurator::GetInstance()->username()), count(0){
    //mode == true时 此时是CharData
    if (mode == true){
        firstClass = "ADOPT_CHAR";
        skuName = "";
        const auto isSnippet = completion[0] == '1';
        if (isSnippet) {
           count = completion.length() - 1;
        } else {
            int index = completion.find(R"(\n)", 0);
            logger::log(std::format("index: {}", index));
            if (index > 1){
                count =  index - 1;
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
void SKU::to_json(nlohmann::json& request){
    nlohmann::json temp_json;
    temp_json = {
        {"begin", begin},
        {"end", end},
        {"user", user},
        {"userType", userType},
        {"count", count},
        {"extra", extra},
        {"type", type},
        {"product", product},
        {"firstClass", firstClass},
        {"secondClass", secondClass},
        {"skuName", skuName},
        {"subType", subType},
    };
    request.push_back(temp_json);
}