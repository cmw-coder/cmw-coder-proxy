#pragma once

#include <magic_enum.hpp>

namespace types::SiVersion {
    enum class Major {
        V35 = 35,
        V40 = 40,
    };

    enum class Minor {
        Unknown = 0,
        V0076 = 76,
        V0084 = 84,
        V0086 = 86,
        V0088 = 88,
        V0096 = 96,
        V0116 = 116,
        V0120 = 120,
        V0124 = 124,
        V0130 = 130,
        V0132 = 132,
    };
}

template<>
struct [[maybe_unused]] magic_enum::customize::enum_range<types::SiVersion::Minor> {
    [[maybe_unused]] static constexpr int min = 0;
    [[maybe_unused]] static constexpr int max = 256;
    /// (max - min) must be less than UINT16_MAX.
};
