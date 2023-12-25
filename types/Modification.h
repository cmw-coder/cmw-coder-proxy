#pragma once

#include <string>
#include <vector>

#include <helpers/WsHelper.h>

#include <types/CaretPosition.h>
#include <types/Key.h>
#include <types/Range.h>


namespace types {
    class Modification {
    public:
        const std::string path;

        explicit Modification(std::string path);

        void add(char character);

        void add(const std::string& characters);

        void navigate(CaretPosition newPosition);

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

        void _syncContent();

        [[nodiscard]] uint32_t _getLineLength(uint32_t lineIndex) const;
        [[nodiscard]] std::pair<uint32_t, uint32_t> _rangeToCharactorOffset(Range range) const;
        std::string _getSelectTabContent(Range range);
    };
}
