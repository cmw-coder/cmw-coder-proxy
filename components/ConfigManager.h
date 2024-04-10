#pragma once

#include <string>

#include <nlohmann/json_fwd.hpp>
#include <singleton_dclp.hpp>

#include <helpers/KeyHelper.h>
#include <types/SiVersion.h>

namespace components {
    class ConfigManager : public SingletonDclp<ConfigManager> {
    public:
        ConfigManager();

        [[nodiscard]] types::SiVersion::Full version() const;

        [[nodiscard]] std::string reportVersion() const;

        void wsSettingSync(nlohmann::json&& data);

    private:
        helpers::KeyHelper::KeyCombination _shortcutManualCompletion;
        std::string _siVersionString;
        types::SiVersion::Full _siVersion;
    };
}
