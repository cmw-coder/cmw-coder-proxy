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

    using Full = std::pair<Major, Minor>;

    template<class T>
    using Map = ConstMap<Major, ConstMap<Minor, T, magic_enum::enum_count<Minor>()>, magic_enum::enum_count<Major>()>;
}