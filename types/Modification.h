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
#include <types/Range.h>


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

        void add(const std::string& characters);

        void navigate(const CaretPosition& newPosition);

        void navigate(Key key);

        void reload();

        void remove();

        void remove(Range range);

        void selectRemove();

        void select(Range range);

        void clearSelect();

        bool isSelect() const;

        void replace(const std::string& characters);

        void replace(Range selectRange, const std::string& characters);

        [[nodiscard]] std::string getText(Range range);

        // bool modifySingle(Type type, CaretPosition modifyPosition, char character = {});

        // bool merge(const Modification&other);

        // [[nodiscard]] nlohmann::json parse() const;

    private:
        CaretPosition _lastPosition;
        Range _lastSelect;
        helpers::WsHelper _wsHelper;
        std::string _content;
        std::vector<uint32_t> _lineOffsets;

        [[nodiscard]] uint32_t _getLineIndent(uint32_t lineIndex) const;

        [[nodiscard]] uint32_t _getLineLength(uint32_t lineIndex) const;

        [[nodiscard]] std::pair<uint32_t, uint32_t> _rangeToCharactorOffset(Range range) const;

        std::string _getSelectTabContent(Range range);

        [[nodiscard]] std::pair<uint32_t, uint32_t> _getLineRange(uint32_t lineIndex) const;

        void _syncContent();
    };
}
