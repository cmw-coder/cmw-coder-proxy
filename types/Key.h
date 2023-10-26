#pragma once

namespace types {
    enum class Key {
        BackSpace = 0x0008,
        Tab = 0x0009,
        Enter = 0x000D,
        Escape = 0x001B,
        Space = 0x0020,
        Tilde = 0x007E,
        S = 0x0053,
        Z = 0x005A,
        RightCurlyBracket = 0x007D,
        F9 = 0x1078,
        F10 = 0x1079,
        F11 = 0x107A,
        F12 = 0x107B,
        Insert = 0x802D,
        Delete = 0x802E,
    };

    enum class Modifier {
        Shift = 0x0300,
        Ctrl = 0x0400,
        Alt = 0x0800,
    };
}