#include <magic_enum.hpp>
#include <ranges>
#include <regex>

#include <types/Modification.h>

using namespace std;
using namespace types;

Modification::Modification(const CaretPosition startPosition, const std::string&content)
    : startPosition(startPosition), type(Type::Addition) {
    _content = views::split(regex_replace(content, regex(R"(\r\n)"s), "\n"s), "\n"sv)
               | views::transform([](auto&&r) { return string(&*r.begin(), ranges::distance(r)); })
               | ranges::to<vector>();
    _updateEndPositionWithContent();
}

Modification::Modification(const CaretPosition startPosition, const CaretPosition endPosition)
    : startPosition(startPosition), endPosition(endPosition), type(Type::Deletion) {
}

Modification::Modification(const uint32_t fromLine, const uint32_t toLine)
    : startPosition(0, fromLine), endPosition(0, toLine), type(Type::Swap) {
}

bool Modification::modifySingle(const Type type, const CaretPosition startPosition, const char character) {
    switch (type) {
        case Type::Addition: {
            if (startPosition < this->startPosition || startPosition > this->endPosition + CaretPosition{1, 0}) {
                return false;
            }
            const auto insertPosition = startPosition - this->startPosition;
            if (character == '\n') {
                // TODO: Deal with auto indentation
                // Split current line into two lines
                const auto newLineContent = _content[insertPosition.line].substr(insertPosition.character);
                _content[insertPosition.line].erase(insertPosition.character);
                _content.insert(
                    _content.begin() + static_cast<int>(insertPosition.line) + 1,
                    newLineContent
                );
            }
            else {
                _content[insertPosition.line].insert(insertPosition.character, 1, character);
            }
            _updateEndPositionWithContent();
            break;
        }
        case Type::Deletion: {
            if (startPosition < this->startPosition || startPosition > this->endPosition + CaretPosition{1, 0}) {
                return false;
            }
            if (startPosition.character == 0) {
                if (startPosition.line == 0) {
                    return false;
                }
                // Merge current line with previous line
                const auto mergePosition = startPosition - this->startPosition;
                _content[mergePosition.line - 1].append(_content[mergePosition.line]);
                _content.erase(_content.begin() + static_cast<int>(mergePosition.line));
            }
            else {
                _content[startPosition.line].erase(startPosition.character, 1);
            }
            _updateEndPositionWithContent();
            break;
        }
        default: {
            return false;
        }
    }
    return true;
}

bool Modification::merge(const Modification&other) {
    switch (other.type) {
        case Type::Addition: {
            if (other.startPosition < startPosition || other.startPosition > endPosition + CaretPosition{1, 0}) {
                return false;
            }
            const auto insertPosition = other.startPosition - startPosition;
            if (other._content.size() == 1) {
                _content[insertPosition.line].insert(
                    insertPosition.character,
                    other._content.front()
                );
            }
            else {
                _content[insertPosition.line].insert(
                    insertPosition.character,
                    other._content.front()
                );
                _content[insertPosition.line].append(
                    other._content.back()
                );
                _content.insert(
                    _content.begin() + static_cast<int>(insertPosition.line) + 1,
                    other._content.begin() + 1,
                    other._content.end() - 1
                );
            }
            _updateEndPositionWithContent();
            break;
        }
        case Type::Deletion: {
            if (other.startPosition < startPosition || other.startPosition > endPosition + CaretPosition{1, 0}) {
                return false;
            }
            // Erase other's range from current content
            const auto eraseStartPosition = other.startPosition - startPosition;
            if (const auto eraseEndPosition = other.endPosition - startPosition;
                eraseStartPosition.line == eraseEndPosition.line) {
                _content[eraseStartPosition.line].erase(
                    eraseEndPosition.character,
                    eraseEndPosition.character - eraseStartPosition.character
                );
            }
            else {
                _content[eraseEndPosition.line] =
                        _content[eraseEndPosition.line].substr(0, eraseEndPosition.character) +
                        _content[eraseStartPosition.line].erase(0, eraseStartPosition.character);
                _content.erase(
                    _content.begin() + static_cast<int>(eraseEndPosition.line) + 1,
                    _content.begin() + static_cast<int>(eraseStartPosition.line)
                );
            }
            _updateEndPositionWithContent();
            break;
        }
        case Type::Swap: {
            if (other.startPosition.line < startPosition.line || other.startPosition.line > endPosition.line ||
                other.endPosition.line < startPosition.line || other.startPosition.line > endPosition.line) {
                return false;
            }
            // Swap lines
            swap(_content[other.startPosition.line], _content[other.endPosition.line]);
            break;
        }
        default: {
            return false;
        }
    }
    return true;
}

nlohmann::json Modification::parse() const {
    nlohmann::json result = {
        {
            "endPosition", {
                {"character", endPosition.character},
                {"line", endPosition.line},
            }
        },
        {
            "startPosition",
            {
                {"character", startPosition.character},
                {"line", startPosition.line},
            }
        },
        {"type", magic_enum::enum_name(type)},
    };
    if (type == Type::Addition) {
        result["content"] = _content;
    }
    return result;
}

void Modification::_updateEndPositionWithContent() {
    endPosition.line = startPosition.line + _content.size() - 1;
    endPosition.character = _content.size() > 1
                                ? _content.back().length()
                                : startPosition.character + _content.back().length();
}
