#include <magic_enum.hpp>

#include <helpers/KeyHelper.h>

using namespace helpers;
using namespace magic_enum;
using namespace std;
using namespace types;

int KeyHelper::toKeycode(types::Key key, types::Modifier modifier) noexcept {
    return enum_integer(key) + enum_integer(modifier);
}

int KeyHelper::toKeycode(types::Key key, std::unordered_set<types::Modifier> modifiers) noexcept {
    auto keycode = enum_integer(key);
    for (const auto &modifier: modifiers) {
        keycode += enum_integer(modifier);
    }
    return keycode;
}
