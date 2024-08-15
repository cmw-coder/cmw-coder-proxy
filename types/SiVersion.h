#pragma once

#include <magic_enum.hpp>

#include <types/ConstMap.h>

namespace types::SiVersion {
    enum class Major {
        V35 = 35,
        V40 = 40,
    };

    enum class Minor {
        Unknown = 0,
        V0086 = 86,
        V0128 = 128,
    };
}

template<>
struct magic_enum::customize::enum_range<types::SiVersion::Minor> {
    static constexpr int min = 0;
    static constexpr int max = 255;
};

namespace types::SiVersion {
    using Full = std::pair<Major, Minor>;

    template<class T>
    using Map = EnumMap<Major, EnumMap<Minor, T>>;
}
