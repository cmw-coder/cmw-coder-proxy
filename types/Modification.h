#pragma once

#include <string>
#include <vector>

#include <helpers/HttpHelper.h>
#include <types/CaretPosition.h>

namespace types {
    class Modification {
    public:
        const std::string path;

        explicit Modification(std::string path);

        bool add(CaretPosition position, char character);

        void reload();

        bool remove(CaretPosition position);

        // bool modifySingle(Type type, CaretPosition modifyPosition, char character = {});

        // bool merge(const Modification&other);

        // [[nodiscard]] nlohmann::json parse() const;

    private:
        std::string _content;
        std::vector<uint32_t> _lineOffsets;

        void _syncContent();
    };
}
