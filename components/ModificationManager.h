#pragma once

#include <shared_mutex>

#include <nlohmann/json.hpp>
#include <singleton_dclp.hpp>

#include <types/CaretPosition.h>
#include <types/Modification.h>

namespace components {
    class ModificationManager : public SingletonDclp<ModificationManager> {
    public:
        ModificationManager() = default;

        ~ModificationManager() override = default;

        void addTab(const std::string& tabName, const std::string& path);

        // TODO: Remove temporary methods
        types::CaretPosition getCurrentPosition() const;

        void interactionAcceptCompletion(const std::any&);

        void interactionCaretUpdate(const std::any& data);

        void interactionDeleteInput(const std::any&);

        void interactionEnterInput(const std::any&);

        void interactionNavigate(const std::any& data);

        void interactionNormalInput(const std::any& data);

        void interactionSave(const std::any&);

        void interactionSelectionClear(const std::any&);

        void interactionSelectionSet(const std::any& data);

        void removeTab(const std::string& tabName);

        bool switchTab(const std::string& tabName);

    private:
        mutable std::shared_mutex _currentModificationMutex;
        std::string _currentTabName;
        std::unordered_map<std::string, types::Modification> _modificationMap;
    };
}
