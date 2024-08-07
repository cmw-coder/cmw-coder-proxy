#pragma once

#include <stdexcept>

namespace types {
    template<typename Key, typename Value, std::size_t Size>
    struct ConstMap {
        std::array<std::pair<Key, Value>, Size> data;

        [[nodiscard]] constexpr Value at(const Key& key) const {
            if (const auto iterator = std::ranges::find_if(data, [&key](const auto& v) { return v.first == key; });
                iterator != end(data)) {
                return iterator->second;
            }
            throw std::out_of_range("Not Found");
        }
    };

    template<class T>
    concept Enumerable = std::is_enum_v<T>;

    template<Enumerable Key, class Value>
    using EnumMap = ConstMap<Key, Value, magic_enum::enum_count<Key>()>;
}
