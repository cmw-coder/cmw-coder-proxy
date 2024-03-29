#include <magic_enum.hpp>

#include <helpers/KeyHelper.h>

using namespace helpers;
using namespace magic_enum;
using namespace std;
using namespace types;

namespace {
    const unordered_map<SiVersion::Major, unordered_map<Key, Keycode>> keyMaps = {
        {
            SiVersion::Major::V35,
            {
                {Key::BackSpace, 0x0008},
                {Key::Tab, 0x0009},
                {Key::Enter, 0x000D},
                {Key::Escape, 0x001B},
                {Key::S, 0x0053},
                {Key::V, 0x0056},
                {Key::Z, 0x005A},
                {Key::RightCurlyBracket, 0x007D},
                {Key::F9, 0x1078},
                {Key::F10, 0x1079},
                {Key::F11, 0x107A},
                {Key::F12, 0x107B},
                {Key::F13, 0x107C},
                {Key::Home, 0x8021},
                {Key::End, 0x8022},
                {Key::PageDown, 0x8023},
                {Key::PageUp, 0x8024},
                {Key::Left, 0x8025},
                {Key::Up, 0x8026},
                {Key::Right, 0x8027},
                {Key::Down, 0x8028},
                {Key::Insert, 0x802D},
                {Key::Delete, 0x802E},
            }
        },
        {
            SiVersion::Major::V40,
            {
                {Key::BackSpace, 0x000008},
                {Key::Tab, 0x000009},
                {Key::Enter, 0x00000D},
                {Key::Escape, 0x00001B},
                {Key::S, 0x000053},
                {Key::V, 0x000056},
                {Key::Z, 0x00005A},
                {Key::RightCurlyBracket, 0x00007D},
                {Key::F9, 0x100078},
                {Key::F10, 0x100079},
                {Key::F11, 0x10007A},
                {Key::F12, 0x10007B},
                {Key::F13, 0x10007C},
                {Key::Home, 0x800021},
                {Key::End, 0x800022},
                {Key::PageDown, 0x800023},
                {Key::PageUp, 0x800024},
                {Key::Left, 0x800025},
                {Key::Up, 0x800026},
                {Key::Right, 0x800027},
                {Key::Down, 0x800028},
                {Key::Insert, 0x80002D},
                {Key::Delete, 0x80002E},
            }
        },
    };
    const unordered_map<SiVersion::Major, unordered_map<Modifier, Keycode>> modifierMaps = {
        {
            SiVersion::Major::V35,
            {
                {Modifier::Shift, 0x0300},
                {Modifier::Ctrl, 0x0400},
                {Modifier::Alt, 0x0800},
            }
        },
        {
            SiVersion::Major::V40,
            {
                {Modifier::Shift, 0x030000},
                {Modifier::Ctrl, 0x040000},
                {Modifier::Alt, 0x080000},
            }
        },
    };
    const unordered_map<SiVersion::Major, Keycode> keyMasks = {
        {SiVersion::Major::V35, 0xF0FF},
        {SiVersion::Major::V40, 0xF0FFFF},
    };
}

KeyHelper::KeyHelper(const SiVersion::Major siVersion) noexcept: _keyMask(keyMasks.at(siVersion)),
                                                                 _keyMap(keyMaps.at(siVersion)),
                                                                 _modifierMap(modifierMaps.at(siVersion)) {}

optional<KeyHelper::KeyCombination> KeyHelper::fromKeycode(const Keycode keycode) const noexcept {
    for (const auto& [key, keyValue]: _keyMap) {
        ModifierSet modifiers{};
        Keycode modifierAccumulated{};
        for (const auto& [modifier, modifierValue]: _modifierMap) {
            if ((keycode & keyValue + modifierValue) == keyValue + modifierValue) {
                modifiers.insert(modifier);
                modifierAccumulated += modifierValue;
            }
        }
        if (keycode == keyValue + modifierAccumulated) {
            return make_pair(key, modifiers);
        }
    }
    return nullopt;
}

bool KeyHelper::isPrintable(const Keycode keycode) const noexcept {
    for (const auto& [modifier, modifierValue]: _modifierMap) {
        if (modifier != Modifier::Shift && (keycode & modifierValue) == modifierValue) {
            return false;
        }
    }
    const auto maskedKeycode = keycode & _keyMask;
    return maskedKeycode >= _printableRange.first && maskedKeycode <= _printableRange.second;
}

Keycode KeyHelper::toKeycode(const Key key, const Modifier modifier) const noexcept {
    return _keyMap.at(key) + _modifierMap.at(modifier);
}

Keycode KeyHelper::toKeycode(const Key key, const std::unordered_set<Modifier>& modifiers) const noexcept {
    auto keycode = _keyMap.at(key);
    for (const auto& modifier: modifiers) {
        keycode += _modifierMap.at(modifier);
    }
    return keycode;
}

char KeyHelper::toPrintable(const Keycode keycode) const noexcept {
    for (const auto& [modifier, modifierValue]: _modifierMap) {
        if (modifier != Modifier::Shift && (keycode & modifierValue) == modifierValue) {
            return false;
        }
    }
    if (const auto maskedKeycode = keycode & _keyMask;
        maskedKeycode >= _printableRange.first && maskedKeycode <= _printableRange.second) {
        return static_cast<char>(maskedKeycode);
    }
    return -1;
}
