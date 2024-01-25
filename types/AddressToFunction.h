#pragma once

#include <utils/system.h>

namespace types {
    template<typename TReturn, typename... TArgs>
    class AddressToFunction final {
        explicit AddressToFunction(TReturn, TArgs...) {}
    };

    template<typename TReturn, typename... TArgs>
    class AddressToFunction<TReturn(TArgs...)> final {
        enum class Type { Cdecl, StdCall };

    public:
        explicit AddressToFunction(const uint32_t address): _address(address) {}

        TReturn operator()(TArgs... args) const {
            if (_callingConvention == Type::Cdecl) {
                return reinterpret_cast<TReturn(__cdecl *)(TArgs...)>(_address)(args...);
            }
            return reinterpret_cast<TReturn(__stdcall *)(TArgs...)>(_address)(args...);
        }

    private:
        static inline Type _callingConvention = get<0>(utils::system::getVersion()) == 4
                                                    ? Type::Cdecl
                                                    : Type::StdCall;
        uint32_t _address;
    };
}
