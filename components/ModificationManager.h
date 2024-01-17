#pragma once

#include <shared_mutex>

#include <nlohmann/json.hpp>
#include <singleton_dclp.hpp>

#include <types/Modification.h>

namespace components {
    class ModificationManager : public SingletonDclp<ModificationManager> {
    public:
        ModificationManager();

        ~ModificationManager() override;

        std::vector<std::string> getRecentFiles(uint32_t limit = 5) const;

        void interactionAcceptCompletion(const std::any&, bool&);

        void interactionDeleteInput(const std::any&, bool&);

        void interactionEnterInput(const std::any&, bool& needBlockMessage);

        void interactionNavigateWithKey(const std::any& data, bool&);

        void interactionNavigateWithMouse(const std::any& data, bool&);

        void interactionNormalInput(const std::any& data, bool&);

        void interactionSave(const std::any&, bool&);

        void interactionSelectionClear(const std::any&, bool&);

        void interactionSelectionSet(const std::any& data, bool&);

    private:
        mutable std::shared_mutex _currentModificationMutex, _modifingFilesMutex;
        std::atomic<bool> _isRunning{true};
        std::unordered_map<std::string, types::Modification> _modificationMap;
        std::unordered_map<std::string, std::chrono::high_resolution_clock::time_point> _recentFiles;

        types::Modification& _currentFile();

        void _monitorCurrentFile();
    };
}
