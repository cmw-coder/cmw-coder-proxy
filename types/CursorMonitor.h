#pragma once

#include <functional>
#include <memory>
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

        void setAction(UserAction userAction);

        template<class T>
        void addHandler(
            const UserAction userAction,
            T* const other,
            void (T::* const memberFunction)(CursorPosition, CursorPosition)
        ) {
            _handlers[userAction] = std::bind_front(memberFunction, other);
        }

    private:
        std::shared_ptr<void> _sharedProcessHandle;
        std::atomic<bool> _isRunning = true;
        std::atomic<CursorPosition> _lastPosition{};
        std::atomic<UserAction> _lastAction{};
        std::unordered_map<UserAction, CallBackFunction> _handlers;
    };
}
