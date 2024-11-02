#pragma once

#include <magic_enum.hpp>
#include <unordered_set>

namespace types {
    enum class Modifier {
        Alt,
        Ctrl,
        Shift,
    };

    using ModifierSet = std::unordered_set<Modifier>;
    using KeyCombination = std::pair<uint32_t, ModifierSet>;
}
