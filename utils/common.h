#pragma once

#include <types/CaretDimension.h>
#include <types/Keys.h>
#include <types/Selection.h>

namespace utils::common {
    bool checkHighestBit(uint16_t value) noexcept;

    uint32_t countLines(const std::string& content);

    types::CaretDimension getCaretDimensions(bool waitTillAvailable = true);

    void insertContent(const std::string& content);

    void replaceContent(const types::Selection& replaceRange, std::string content);

    uint32_t toKeycode(char key, types::Modifier modifier);

    uint32_t toKeycode(char key, const types::ModifierSet& modifiers);

    std::string uuid();
}
