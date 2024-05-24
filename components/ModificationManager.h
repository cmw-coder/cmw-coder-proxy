#pragma once

#include <shared_mutex>

#include <singleton_dclp.hpp>

namespace components {
    class ModificationManager : public SingletonDclp<ModificationManager> {
    public:
        ModificationManager();

        ~ModificationManager() override;

        std::vector<std::filesystem::path> getRecentFiles(uint32_t limit = 5) const;

    private:
        mutable std::shared_mutex _modifingFilesMutex;
        std::atomic<bool> _isRunning{true};
        std::unordered_map<std::filesystem::path, std::chrono::high_resolution_clock::time_point> _recentFiles;

        void _monitorCurrentFile();
    };
}
