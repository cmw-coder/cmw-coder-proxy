#pragma once

#include <types/CaretDimension.h>

namespace utils::common {
    bool checkKeyIsUp(unsigned short keyState) noexcept;

    types::CaretDimension getCaretDimensions(bool waitTillAvailable = true);

    std::string uuid();
}
