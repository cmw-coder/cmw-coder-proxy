#pragma once

#include <unordered_map>
#include <unordered_set>

#include <types/common.h>
#include <types/Key.h>
#include <types/SiVersion.h>

namespace helpers {
    class KeyHelper {
        using ModifierSet = std::unordered_set<types::Modifier>;
        using KeyCombination = std::pair<types::Key, ModifierSet>;

    public:
        explicit KeyHelper(types::SiVersion::Major siVersion) noexcept;

        [[nodiscard]] std::optional<KeyCombination> fromKeycode(types::Keycode keycode) const noexcept;

        [[nodiscard]] bool isPrintable(types::Keycode keycode) const noexcept;

        [[nodiscard]] types::Keycode toKeycode(types::Key key, types::Modifier modifier) const noexcept;

        [[nodiscard]] types::Keycode toKeycode(types::Key key, const ModifierSet&modifiers = {}) const noexcept;

        [[nodiscard]] char toPrintable(types::Keycode keycode) const noexcept;

    private:
        const types::Keycode _keyMask;
        const std::pair<types::Keycode, types::Keycode> _printableRange = {0x000020, 0x00007E};
        const std::unordered_map<types::Key, types::Keycode> _keyMap;
        const std::unordered_map<types::Modifier, types::Keycode> _modifierMap;
    };
}
