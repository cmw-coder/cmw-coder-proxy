#pragma once

#include <string>
#include <vector>

#include <helpers/WsHelper.h>
#include <types/CaretPosition.h>
#include <types/Key.h>

namespace types {
    class Modification {
    public:
        const std::string path;

        explicit Modification(std::string path);

        void add(char character);

        void navigate(CaretPosition newPosition);

        void navigate(Key key);

        void reload();

        void remove();

        // bool modifySingle(Type type, CaretPosition modifyPosition, char character = {});

        // bool merge(const Modification&other);

        // [[nodiscard]] nlohmann::json parse() const;

    private:
        CaretPosition _lastPosition;
        helpers::WsHelper _wsHelper;
        std::string _content;
        std::vector<uint32_t> _lineOffsets;

        void _syncContent();

        [[nodiscard]] uint32_t _getLineLength(uint32_t lineIndex) const;
    };
}
