#pragma once

#include <filesystem>
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
        mutable std::shared_mutex _currentFilePathMutex, _currentProjectPathMutex, _shortcutMutex;
        helpers::KeyHelper::KeyCombination _shortcutCommit, _shortcutManualCompletion;
        std::atomic<bool> _isRunning{true};
        std::filesystem::path _currentFilePath, _currentProjectPath;
        std::string _siVersionString;
        types::SiVersion::Full _siVersion;

        void _threadRetrieveProjectDirectory();

        void _threadRetrieveSvnDirectory();
    };
}
