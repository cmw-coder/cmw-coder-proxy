#pragma once

#include <shared_mutex>

#include <nlohmann/json.hpp>
#include <singleton_dclp.hpp>

#include <types/Modification.h>

namespace components {
    class ModificationManager : public SingletonDclp<ModificationManager> {
    public:
        ModificationManager() = default;

        ~ModificationManager() override = default;

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
        mutable std::shared_mutex _currentModificationMutex;
        std::unordered_map<std::string, types::Modification> _modificationMap;

        types::Modification& _currentFile();
    };
}
