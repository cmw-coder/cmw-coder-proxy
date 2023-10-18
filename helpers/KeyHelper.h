#pragma once

#include <unordered_set>

#include <types/Key.h>

namespace helpers {
    class KeyHelper {
    public:
        static int toKeycode(types::Key key, types::Modifier modifier) noexcept;

        static int toKeycode(types::Key key, std::unordered_set<types::Modifier> modifiers = {}) noexcept;
    };
}