#pragma once

#include <string>
#include <ctime>
#include <nlohmann/json.hpp>

namespace types {
    class SKU {
        public:
            SKU(){};
            SKU(std::string user, std::string projectId, std::string completion, bool mode);
            void to_json(nlohmann::json& request);

        private:
            std::time_t begin;       //起始时间
            std::time_t end;         //结束时间
            std::string subType; //项目ID
            std::string user; //用户名
            int count;
            std::string firstClass; //大类，"ADOPT_CHAR" 行相关用CODE;如CODE.CMW.GENE(ADOPT)，字符数GENE_CHAR(ADOPT_CHAR)
            std::string skuName; //sku名称 GENE、ADOPT
            static inline const std::string extra = "SI-0.6.1"; //插件版本号
            static inline const std::string secondClass = "CMW"; //小类 CMW
            static inline const std::string userType = "USER"; //用户类型"USER"
            static inline const std::string type = "AIGC";   //类型：固定AIGC
            static inline const std::string product = "SI"; //产品名：固定 SI
    };
            

}