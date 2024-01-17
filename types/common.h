#pragma once

#include <chrono>

namespace types {
    constexpr auto UM_KEYCODE = 0x07E9;

    template<typename TReturn, typename... TArgs>
    class StdCallFunction {
        explicit StdCallFunction(TReturn, TArgs...) {}
    };

    template<typename TReturn, typename... TArgs>
    class StdCallFunction<TReturn(TArgs...)> final {
    public:
        explicit StdCallFunction(const uint32_t address) : _address(address) {}

        TReturn operator()(TArgs... args) const {
            return reinterpret_cast<TReturn(__cdecl *)(TArgs...)>(_address)(args...);//__cdecl
        }

    private:
        uint32_t _address;
    };

    using Keycode = unsigned int;
    using Time = std::chrono::time_point<std::chrono::high_resolution_clock>;
}
