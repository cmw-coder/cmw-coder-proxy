#include <ctime>

#include <types/Statistics.h>
#include <types/Configurator.h>


using namespace types;

SKU::SKU(std::string user, std::string projectId, std::string completion, bool mode):begin(time(0)), end(time(0)), subType(projectId), 
user(Configurator::GetInstance()->username()), count(completion.length() - 1){
    //mode == true时 此时是CharData
    if (mode == true){
        firstClass = "ADOPT_CHAR";
        skuName = "";
    } else {
        firstClass = "CODE";
        skuName = "ADOPT";
    }
    
}
SKU::to_json(nlohmann::json& request){
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
        {"subType", subType}.
    };
    request.push_back(temp_json)
}