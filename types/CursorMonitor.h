#pragma once

#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <unordered_map>

#include <singleton_dclp.hpp>

#include <types/CursorPosition.h>
#include <types/UserAction.h>

namespace types {
    class CursorMonitor : public SingletonDclp<CursorMonitor> {
    public:
        using CallBackFunction = std::function<void(CursorPosition, CursorPosition)>;

        CursorMonitor();

        ~CursorMonitor() override;

        void queueAction(UserAction userAction);

        template<class T>
        void addHandler(
                UserAction userAction,
                T *const other,
                void(T::* const memberFunction)(CursorPosition, CursorPosition)
        ) {
            _handlers[userAction] = std::bind_front(memberFunction, other);
        }

    private:
        const uint64_t _baseAddress;
        std::shared_ptr<void> _sharedProcessHandle;
        std::atomic<bool> _isRunning = true;
        std::atomic<CursorPosition> _lastPosition{};
        std::atomic<UserAction> _lastAction{};
        std::unordered_map<UserAction, CallBackFunction> _handlers;
    };

}