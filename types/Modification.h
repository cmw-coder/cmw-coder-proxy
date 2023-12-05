#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

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

        bool modifySingle(Type type, CaretPosition modifyPosition, char character = {});

        bool merge(const Modification&other);

        [[nodiscard]] nlohmann::json parse() const;

    private:
        std::vector<std::string> _content;

        void _updateEndPositionWithContent();
    };
}
