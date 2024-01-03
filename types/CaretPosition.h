#pragma once

#include <cstdint>

namespace types {
    class CaretPosition {
    public:
        uint32_t character, line, maxCharacter;

        CaretPosition() = default;

        CaretPosition(uint32_t character, uint32_t line);

        CaretPosition& addCharactor(int64_t charactor);

        CaretPosition& addLine(int64_t line);

        void reset();

        bool operator<(const CaretPosition& other) const;

        bool operator<=(const CaretPosition& other) const;

        bool operator==(const CaretPosition& other) const;

        bool operator!=(const CaretPosition& other) const;

        bool operator>(const CaretPosition& other) const;

        bool operator>=(const CaretPosition& other) const;

        CaretPosition& operator+=(const CaretPosition& other);

        CaretPosition& operator-=(const CaretPosition& other);

        friend CaretPosition operator+(const CaretPosition& lhs, const CaretPosition& rhs);

        friend CaretPosition operator-(const CaretPosition& lhs, const CaretPosition& rhs);
    };
}

// #include <format>
// #include <string>
//
// template<>
// struct std::formatter<types::CaretPosition> : std::formatter<std::string> {
//     bool detailed{false};
//
//     constexpr auto parse(std::format_parse_context& context) {
//         auto it = context.begin();
//         const auto end = context.end();
//         if (it != end && (*it == 'm')) detailed = (*it++) == 'm';
//         if (it != end && *it != '}') throw format_error("Invalid format");
//         return it;
//     }
//
//     template<class FormatContext>
//     auto format(const types::CaretPosition& CaretPosition, FormatContext& context) {
//         std::string base_str = "line: " + to_string(CaretPosition.line) + " character: " + to_string(
//                                    CaretPosition.character);
//         if (detailed) {
//             std::string op_str = base_str + " maxCharacter: " + to_string(CaretPosition.maxCharacter);
//             return formatter<std::string>::format(op_str, context);
//         }
//         return formatter<std::string>::format(base_str, context);
//     }
// };
