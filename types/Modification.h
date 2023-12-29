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

        void remove(const Range& range);

        void selectRemove();

        void select(const Range& range);

        void clearSelect();

        bool isSelect() const;

        void replace(const std::string& characters);

        void replace(const Range& selectRange, const std::string& characters);


    private:
        CaretPosition _lastPosition;
        Range _lastSelect;
        helpers::WsHelper _wsHelper;
        std::string _content;
        std::vector<uint32_t> _lineOffsets;

        [[nodiscard]] std::string _addIndentOnSelection(const Range& range) const;

        [[nodiscard]] uint32_t _getLineIndent(uint32_t lineIndex) const;

        [[nodiscard]] uint32_t _getLineLength(uint32_t lineIndex) const;

        [[nodiscard]] std::pair<uint32_t, uint32_t> _getLineOffsets(uint32_t lineIndex) const;

        [[nodiscard]] std::string _getRangeContent(const Range& range) const;

        [[nodiscard]] std::pair<uint32_t, uint32_t> _getRangeOffsets(const Range& range) const;

        void _syncContent();
    };
}
