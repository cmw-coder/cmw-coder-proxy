#pragma once

#include <cstdint>
#include <string>

namespace types {
    class CompactString {
        struct Data {
            uint8_t lengthLow, lengthHigh;
            char content[4092];
        };

    public:
        [[nodiscard]] std::string str() const;

        Data* data();

    private:
        Data _data{};
    };
}
