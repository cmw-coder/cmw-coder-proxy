#pragma once

#include <unordered_set>

namespace types {
    enum class Key {
        BackSpace,
        Tab,
        Enter,
        Escape,
        A,
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
        RightCurlyBracket,
        F9,
        F10,
        F11,
        F12,
        F13,
        Home,
        End,
        PageDown,
        PageUp,
        Left,
        Up,
        Right,
        Down,
        Insert,
        Delete,
    };

    enum class Modifier {
        Shift,
        Ctrl,
        Alt,
    };

    using ModifierSet = std::unordered_set<Modifier>;
}
