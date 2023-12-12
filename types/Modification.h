#pragma once

#include <string>
#include <vector>

#include <helpers/HttpHelper.h>
#include <types/CaretPosition.h>
#include <types/Key.h>

namespace types {
    class Modification {
    public:
        const std::string path;

        explicit Modification(std::string path);

        void add(char character);

        bool flush();

        void navigate(Key key);

        void reload();

        void remove();

        // bool modifySingle(Type type, CaretPosition modifyPosition, char character = {});

        // bool merge(const Modification&other);

        // [[nodiscard]] nlohmann::json parse() const;

    private:
        CaretPosition _lastPosition;
        std::string _buffer;
        std::vector<std::string> _content;
        uint32_t _removeCount{};

        void _syncContent();
    };
}
