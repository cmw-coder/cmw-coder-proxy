#pragma once

#include <types/CaretPosition.h>

namespace types {
    class Range {
    public:
        CaretPosition start, end;

        Range() = default;

        Range(const CaretPosition& start, const CaretPosition& end);

        Range(uint32_t startLine, uint32_t startCharacter, uint32_t endLine, uint32_t endCharacter);

        [[nodiscard]] bool isEmpty() const;

        [[nodiscard]] bool isSingleLine() const;

        [[nodiscard]] bool contains(const Range& other) const;

        [[nodiscard]] bool isEqual(const Range& other) const;

        [[nodiscard]] bool isBefore(const Range& other) const;

        [[nodiscard]] Range Union(const Range& other) const;

        [[nodiscard]] Range intersection(const Range& other) const;
    };
}


#include <format>
#include <string>

template<>
struct std::formatter<types::Range> : std::formatter<std::string> {
    template<class FormatContext>
    auto format(const types::Range& Range, FormatContext& context) {
        std::string base_str = std::format("Start: {}, End: {}", Range.start, Range.end);
        return formatter<std::string>::format(base_str, context);
    }
};
