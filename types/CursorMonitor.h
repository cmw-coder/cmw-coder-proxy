#pragma once

#include <functional>
#include <memory>
#include <vector>

namespace types {
    class CursorMonitor {
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

        using CallBackFunction = std::function<void(CursorPosition)>;

        CursorMonitor();

        ~CursorMonitor();

        template<class T>
        void addHandler(T *const other, CallBackFunction memberFunction) {
            _handlers.emplace_back(std::bind_front(memberFunction, other));
        }

        void addHandler(CallBackFunction function);

    private:
        std::atomic<bool> _isRunning = true;
        std::atomic<CursorPosition> _lastPosition{};
        std::vector<CallBackFunction> _handlers;
        std::shared_ptr<void> _sharedProcessHandle;

        void _notify();
    };

}