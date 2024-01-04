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
        CompactString() = default;

        explicit CompactString(std::string_view str);

        [[nodiscard]] std::string str() const;

        Data* data();

    private:
        Data _data{};
    };
}
