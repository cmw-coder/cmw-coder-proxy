#pragma once

#include <queue>

#include <nlohmann/json.hpp>
#include <singleton_dclp.hpp>

#include <types/Modification.h>

namespace components {
    class ModificationManager : public SingletonDclp<ModificationManager> {
    public:
        ModificationManager() = default;

        ~ModificationManager() override = default;

        void clearHistory();

        void deleteInput(types::CaretPosition position);

        nlohmann::json getHistory() const;

        void normalInput(types::CaretPosition position, char character);

    private:
        mutable std::shared_mutex _bufferMutex, _historyMutex;
        std::vector<types::Modification> _historyQueue;
        std::optional<types::Modification> _buffer{std::nullopt};
    };
}
