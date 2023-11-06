#pragma once

#include <unordered_set>

#include <types/Key.h>
#include <types/SiVersion.h>

namespace helpers {
    class KeyHelper {
    private:
        using ModifierSet = std::unordered_set<types::Modifier>;

    public:
        explicit KeyHelper(types::SiVersion::Major siVersion) noexcept;

        [[nodiscard]] std::optional<std::pair<types::Key, ModifierSet>> fromKeycode(int keycode) const noexcept;

        [[nodiscard]] bool isNavigate(int keycode) const noexcept;

        [[nodiscard]] bool isPrintable(int keycode) const noexcept;

        [[nodiscard]] int toKeycode(types::Key key, types::Modifier modifier) const noexcept;

        [[nodiscard]] int toKeycode(types::Key key, const ModifierSet &modifiers = {}) const noexcept;

    private:
        const int _keyMask;
        const std::pair<int, int> _navigateRange, _printableRange;
        const std::unordered_map<types::Key, int> _keyMap;
        const std::unordered_map<types::Modifier, int> _modifierMap;
    };
}