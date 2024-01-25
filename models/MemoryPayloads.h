#pragma once

#include <cstdint>
#include <string>

namespace models {
    class SimpleString {
        struct Data {
            uint8_t lengthLow, lengthHigh;
            char content[4092];
        };

    public:
        SimpleString() = default;

        explicit SimpleString(std::string_view str);

        [[nodiscard]] std::string str() const;

        Data* data();

    private:
        Data _data{};
    };
}
