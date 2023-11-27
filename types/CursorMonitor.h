#pragma once

#include <functional>
#include <memory>
#include <unordered_map>

#include <types/CursorPosition.h>
#include <types/UserAction.h>

namespace types {
    class CursorMonitor {
    public:
        using CallBackFunction = std::function<void(CursorPosition, CursorPosition)>;

        CursorMonitor();

        void queueAction(UserAction userAction);

    private:
        std::shared_ptr<void> _processHandle;
        std::atomic<bool> _isRunning = true;
        std::atomic<CursorPosition> _lastPosition{};
        std::atomic<UserAction> _lastAction{};
        std::unordered_map<UserAction, CallBackFunction> _handlers;
    };
}
