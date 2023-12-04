#pragma once

#include <string>
#include <vector>

#include <types/CaretPosition.h>

namespace types {
    class Modification {
    public:
        enum class Type {
            Addition,
            Deletion,
            Swap
        };

        const CaretPosition startPosition;
        CaretPosition endPosition;
        const Type type;

        explicit Modification(CaretPosition startPosition, const std::string&content = "");

        Modification(CaretPosition startPosition, CaretPosition endPosition);

        Modification(uint32_t fromLine, uint32_t toLine);

        bool modifySingle(Type type, CaretPosition startPosition, char character = {});

        bool merge(const Modification&other);

    private:
        std::vector<std::string> _content;

        void _updateEndPositionWithContent();
    };
}
