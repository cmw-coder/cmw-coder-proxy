#pragma once

#include <chrono>

namespace types {
    constexpr auto UM_KEYCODE = 0x07E9;

    using Keycode = unsigned int;
    using Time = std::chrono::time_point<std::chrono::high_resolution_clock>;
}
