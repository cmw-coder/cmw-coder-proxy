#pragma once

#include <types/CaretPosition.h>

namespace types {
    class Selection {
    public:
        CaretPosition begin, end;

        Selection() = default;

        Selection(const CaretPosition& begin, const CaretPosition& end);

        [[nodiscard]] bool hasIntersection(const Selection& other) const;

        [[nodiscard]] bool isEmpty() const;

        [[nodiscard]] bool isSingleLine() const;

        [[nodiscard]] bool isEqual(const Selection& other) const;

        void reset();
    };
}
