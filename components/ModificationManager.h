#pragma once

#include <queue>

#include <singleton_dclp.hpp>

#include <types/Modification.h>

namespace components {
    class ModificationManager : public SingletonDclp<ModificationManager> {
    public:
        ModificationManager() = default;

        ~ModificationManager() override = default;

        void deleteInput(types::CaretPosition position);

        void normalInput(types::CaretPosition position, char character);

    private:
        mutable std::shared_mutex _bufferMutex, _historyMutex;
        std::queue<types::Modification> _historyQueue;
        std::optional<types::Modification> _buffer{std::nullopt};
    };
}
