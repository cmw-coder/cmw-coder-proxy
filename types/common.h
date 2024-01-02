#pragma once

#include <chrono>

#include <types/SiVersion.h>

namespace types {
    constexpr auto UM_KEYCODE = 0x07E9;

    template<typename TReturn, typename... TArgs>
    class StdCallFunction {};

    template<typename TReturn, typename... TArgs>
    class StdCallFunction<TReturn(TArgs...)> final {
    public:
        explicit StdCallFunction(const uint32_t address) : _address(address) {}

        TReturn operator()(TArgs... args) const {
            return reinterpret_cast<TReturn(__stdcall *)(TArgs...)>(_address)(args...);
        }

    private:
        uint32_t _address;
    };

    using EditorVersion = std::pair<SiVersion::Major, SiVersion::Minor>;
    using Keycode = unsigned int;
    using Time = std::chrono::time_point<std::chrono::high_resolution_clock>;
}
