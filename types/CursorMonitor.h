#pragma once

#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <unordered_map>

#include <singleton_dclp.hpp>

#include <types/UserAction.h>

namespace types {
    class CursorMonitor : public SingletonDclp<CursorMonitor> {
    public:
        class CursorPosition {
        public:
            uint32_t line;
            uint32_t character;

            bool operator==(const CursorPosition &other) const {
                return this->line == other.line && this->character == other.character;
            }

            bool operator!=(const CursorPosition &other) const {
                return !(*this == other);
            }
        };

        using CallBackFunction = std::function<void(CursorPosition, CursorPosition)>;

        CursorMonitor();

        ~CursorMonitor() override;

        void queueAction(UserAction userAction);

        template<class T>
        void addHandler(UserAction userAction, T *const other, CallBackFunction memberFunction) {
            _handlers[userAction] = std::bind_front(memberFunction, other);
        }

        void addHandler(UserAction userAction, CallBackFunction function);

    private:
        const uint64_t _baseAddress;
        std::shared_ptr<void> _sharedProcessHandle;
        std::atomic<bool> _isRunning = true;
        std::atomic<CursorPosition> _lastPosition{};
        mutable std::shared_mutex _actionQueueMutex{}, _positionQueueMutex{};
        std::deque<UserAction> _actionQueue{};
        std::deque<std::pair<CursorPosition, CursorPosition>> _positionQueue{};
        std::unordered_map<UserAction, CallBackFunction> _handlers;
    };

}