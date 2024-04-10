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

        ~ConfigManager() override;

        [[nodiscard]] bool checkCommit(
            types::Key key,
            const helpers::KeyHelper::ModifierSet& modifiers
        ) const;

        [[nodiscard]] bool checkManualCompletion(
            types::Key key,
            const helpers::KeyHelper::ModifierSet& modifiers
        ) const;

        [[nodiscard]] types::SiVersion::Full version() const;

        [[nodiscard]] std::string reportVersion() const;

        void wsSettingSync(nlohmann::json&& data);

    private:
        mutable std::shared_mutex _currentProjectMutex, _shortcutMutex;
        helpers::KeyHelper::KeyCombination _shortcutCommit, _shortcutManualCompletion;
        std::unique_ptr<helpers::KeyHelper> _keyHelper;
        std::atomic<bool> _isRunning{true};
        std::string _currentProject, _siVersionString;
        types::SiVersion::Full _siVersion;

        void _threadRetrieveProjectDirectory();
    };
}
