#pragma once

#include <chrono>
#include <map>
#include <components/Configurator.h>

#include <types/SiVersion.h>

namespace types {

    enum class CallingConventionType {
        StdCall,
        Cdecl
    };

    inline std::map<SiVersion::Major, CallingConventionType> callingConventionMap = {
        {SiVersion::Major::V35, CallingConventionType::StdCall},
        {SiVersion::Major::V40, CallingConventionType::Cdecl}
    };

    constexpr auto UM_KEYCODE = 0x07E9;

    template<typename TReturn, typename... TArgs>
    class StdCallFunction {
        explicit StdCallFunction(TReturn, TArgs...) {}
    };

    template<typename TReturn, typename... TArgs>
    class StdCallFunction<TReturn(TArgs...)> final {
    public:

        static CallingConventionType callConvention;
        explicit StdCallFunction(const uint32_t address) :
            _address(address),
            _isCdecl(components::Configurator::GetInstance()->version().first == SiVersion::Major::V40) {}

        TReturn operator()(TArgs... args) const {
            if (_isCdecl) {
                return reinterpret_cast<TReturn(__cdecl *)(TArgs...)>(_address)(args...);
            } else {
                return reinterpret_cast<TReturn(__stdcall *)(TArgs...)>(_address)(args...);
            }
        }

    private:
        bool _isCdecl;
        uint32_t _address;
    };

    using Keycode = unsigned int;
    using Time = std::chrono::time_point<std::chrono::high_resolution_clock>;
}
