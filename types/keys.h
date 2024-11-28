#pragma once

#include <magic_enum/magic_enum.hpp>
#include <unordered_set>

namespace types {
    enum class Modifier {
        Alt,
        Ctrl,
        Shift,
    };

    using ModifierSet = std::unordered_set<Modifier>;
    using KeyCombination = std::pair<uint32_t, ModifierSet>;

    inline std::string stringifyKeyCombination(const KeyCombination& keyCombination) {
        using namespace magic_enum;
        std::string result;
        for (const auto& modifier: keyCombination.second) {
            result += enum_name(modifier);
            result += " + ";
        }
        result += std::to_string(keyCombination.first);
        return result;
    }
}
