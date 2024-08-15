#pragma once

#include <types/CaretDimension.h>

namespace utils::common {
    bool checkHighestBit(uint16_t value) noexcept;

    types::CaretDimension getCaretDimensions(bool waitTillAvailable = true);

    std::string uuid();
}
