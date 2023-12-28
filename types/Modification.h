/**
 * @file Modification.h
 * @brief Contains the Modification class which is used to modify a file's content.
 */

#pragma once

#include <string>
#include <vector>

#include <helpers/WsHelper.h>
#include <types/CaretPosition.h>
#include <types/Key.h>

namespace types {
    /**
     * @class Modification
     * @brief A class to record modifications of a file's content.
     */
    class Modification {
    public:
        const std::string path;

        explicit Modification(std::string path);

        void add(char character);

        void add(std::string characters);

        void navigate(const CaretPosition& newPosition);

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

        [[nodiscard]] uint32_t _getLineIndent(uint32_t lineIndex) const;

        [[nodiscard]] uint32_t _getLineLength(uint32_t lineIndex) const;

        [[nodiscard]] std::pair<uint32_t, uint32_t> _getLineRange(uint32_t lineIndex) const;

        void _syncContent();
    };
}
