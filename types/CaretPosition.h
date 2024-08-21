#pragma once

#include <cstdint>

namespace types {
    class CaretPosition {
    public:
        uint32_t character, line, maxCharacter;

        CaretPosition() = default;

        CaretPosition(uint32_t character, uint32_t line);

        void reset();

        bool operator<(const CaretPosition& other) const;

        bool operator<=(const CaretPosition& other) const;

        bool operator==(const CaretPosition& other) const;

        bool operator!=(const CaretPosition& other) const;

        bool operator>(const CaretPosition& other) const;

        bool operator>=(const CaretPosition& other) const;

        friend CaretPosition operator+(const CaretPosition& lhs, const CaretPosition& rhs);

        friend CaretPosition operator-(const CaretPosition& lhs, const CaretPosition& rhs);
    };
}
