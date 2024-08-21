#pragma once

#include <chrono>

namespace types {
    constexpr auto UM_KEYCODE = 0x07E9;

    template<class... Ts>
    struct Overloaded : Ts... {
        using Ts::operator()...;
    };

    using Keycode = unsigned int;
    using Time = std::chrono::time_point<std::chrono::high_resolution_clock>;
}
