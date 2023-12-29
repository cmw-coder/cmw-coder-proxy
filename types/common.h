#pragma once

#include <chrono>

#include <types/SiVersion.h>

namespace types {
    constexpr auto UM_KEYCODE = 0x07E9;

    using Keycode = unsigned int;
    using Time = std::chrono::time_point<std::chrono::high_resolution_clock>;
    using EditorVersion = std::pair<SiVersion::Major, SiVersion::Minor>;
}
