#pragma once

namespace types {
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
}
