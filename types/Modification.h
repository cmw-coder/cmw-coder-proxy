/**
 * @file Modification.h
 * @brief Contains the Modification class which is used to modify a file's content.
 */

#pragma once

#include <string>
#include <vector>

#include <types/CaretPosition.h>
#include <types/Key.h>
#include <types/Selection.h>


namespace types {
    /**
     * @class Modification
     * @brief A class to record modifications of a file's content.
     */
    class Modification {
    public:
        const std::string path;

        explicit Modification(std::string path);

        void acceptCompletion();

        void add(char character);

        void add(const std::string& characters);

        // TODO: Remove temporary methods
        [[nodiscard]] CaretPosition getPosition() const;

        void navigate(const CaretPosition& newPosition);

        void navigate(Key key);

        void reload();

        void remove();

        void selectionClear();

        void selectionSet(const Selection& range);

    private:
        CaretPosition _lastPosition{};
        Selection _lastSelect{};
        std::string _content;
        std::vector<uint32_t> _lineOffsets;

        [[nodiscard]] std::string _addRangeIndent(const Selection& range) const;

        [[nodiscard]] std::string _getLineContent(uint32_t lineIndex) const;

        [[nodiscard]] uint32_t _getLineIndent(uint32_t lineIndex) const;

        [[nodiscard]] uint32_t _getLineLength(uint32_t lineIndex) const;

        [[nodiscard]] std::pair<uint32_t, uint32_t> _getLineOffsets(uint32_t lineIndex) const;

        [[nodiscard]] std::string _getRangeContent(const Selection& range) const;

        [[nodiscard]] std::pair<uint32_t, uint32_t> _getRangeOffsets(const Selection& range) const;

        void _setRangeContent(const Selection& range, const std::string& characters = {});
    };
}
