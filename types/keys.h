#pragma once

#include <magic_enum.hpp>
#include <unordered_set>

namespace types {
    enum class Key : unsigned {
        Unknown = 0x00,
        BackSpace = 0x08,
        Tab,
        Enter = 0x0D,
        Escape = 0x1B,
        PageUp = 0x21,
        PageDown,
        End,
        Home,
        Left,
        Up,
        Right,
        Down,
        Insert = 0x2D,
        Delete,
        A = 0x41,
        B,
        C,
        D,
        E,
        F,
        G,
        H,
        I,
        J,
        K,
        L,
        M,
        N,
        O,
        P,
        Q,
        R,
        S,
        T,
        U,
        V,
        W,
        X,
        Y,
        Z,
        F9 = 0x78,
        F10,
        F11,
        F12,
        F13,
        RightCurlyBracket = 0xDD,
    };

    enum class Modifier {
        Shift,
        Ctrl,
        Alt,
    };

    using ModifierSet = std::unordered_set<Modifier>;
}

template<>
struct magic_enum::customize::enum_range<types::Key> {
    static constexpr int min = 0;
    static constexpr int max = 255;
};