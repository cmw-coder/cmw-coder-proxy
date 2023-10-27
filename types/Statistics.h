#pragma once

#include <chrono>
#include <string>
#include <nlohmann/json.hpp>

namespace types {
    class Statistics {
    public:
        Statistics(std::string projectId, std::string completion, bool mode);

        [[nodiscard]] nlohmann::json parse();

    private:
        int64_t _beginEpochSeconds,_endEpochSeconds;
        std::string _projectId; //项目ID
        std::string user; //用户名
        uint64_t count{};
        std::string firstClass; //大类，"ADOPT_CHAR" 行相关用CODE;如CODE.CMW.GENE(ADOPT)，字符数GENE_CHAR(ADOPT_CHAR)
        std::string skuName; //sku名称 GENE、ADOPT
        static inline const std::string extra = "SI-0.6.1"; //插件版本号
    };


}