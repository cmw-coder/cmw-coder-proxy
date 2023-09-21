#pragma once

#include <functional>
#include <memory>
#include <optional>

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

        CursorMonitor();

        std::optional<std::pair<CursorPosition, CursorPosition>> checkPosition();

    private:
        const uint64_t _baseAddress;
        std::shared_ptr<void> _sharedProcessHandle;
        CursorPosition _lastPosition{};
    };

}