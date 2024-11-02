#pragma once

#include <filesystem>
#include <string>

#include <singleton_dclp.hpp>

#include <models/configs.h>
#include <types/SiVersion.h>


namespace components {
    class ConfigManager : public SingletonDclp<ConfigManager> {
    public:
        ConfigManager();

        ~ConfigManager() override;

        [[nodiscard]] models::CompletionConfig getCompletionConfig() const;

        [[nodiscard]] types::SiVersion::Full version() const;

        [[nodiscard]] std::string reportVersion() const;

    private:
        mutable std::shared_mutex _currentFilePathMutex, _currentProjectPathMutex;
        std::atomic<bool> _isRunning{true};
        std::filesystem::path _currentFilePath, _currentProjectPath;
        std::string _siVersionString;
        types::SiVersion::Full _siVersion;

        void _threadMonitorCurrentFilePath();

        void _threadMonitorCurrentProjectPath();
    };
}
