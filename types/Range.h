#pragma once

#include <types/CaretPosition.h>

namespace types {
    class Range {
    public:
        CaretPosition end, start;

        Range() = default;

        Range(const CaretPosition& start, const CaretPosition& end);

        [[nodiscard]] bool contains(const Range& other) const;

        [[nodiscard]] bool isEmpty() const;

        [[nodiscard]] bool isSingleLine() const;

        [[nodiscard]] bool isEqual(const Range& other) const;

        [[nodiscard]] bool isBefore(const Range& other) const;

        void reset();
    };
}

// #include <format>
// #include <string>
//
// template<>
// struct std::formatter<types::Range> : std::formatter<std::string> {
//     template<class FormatContext>
//     auto format(const types::Range& Range, FormatContext& context) {
//         std::string base_str = std::format("Start: {}, End: {}", Range.start, Range.end);
//         return formatter<std::string>::format(base_str, context);
//     }
// };
