#include <magic_enum.hpp>
#include <ranges>
#include <regex>
#include <utility>

#include <types/Modification.h>
#include <utils/crypto.h>
#include <utils/fs.h>
#include <utils/logger.h>

using namespace helpers;
using namespace std;
using namespace types;
using namespace utils;

Modification::Modification(string path): path(move(path)) {
    reload();
}

bool Modification::add(const CaretPosition position, const char character) {
    try {
        const auto charactorOffset = _lineOffsets.at(position.line) + position.character;
        for (auto& offset: _lineOffsets | views::drop(position.line + 1)) {
            offset += 1;
        }
        _content.insert(charactorOffset, 1, character);
        if (character == '\n') {
            // Insert new line
            _lineOffsets.insert(
                _lineOffsets.begin() + static_cast<int>(position.line) + 1,
                charactorOffset + 1
            );
        }
    } catch (out_of_range&) {
        return false;
    }
    _syncContent();
    return true;
}

void Modification::reload() {
    logger::debug(format("Reloading with path: '{}'", path));
    logger::debug(format("Old content: \n{}", _content));
    _content = fs::readFile(path);
    _syncContent();
    _lineOffsets.clear();
    logger::debug(format("New content: \n{}", _content));
    // Calculate each line's offset
    _lineOffsets.push_back(0);
    for (const auto& line: _content | views::split('\n')) {
        _lineOffsets.push_back(_lineOffsets.back() + line.size() + 1);
    }
    _lineOffsets.pop_back();
}

bool Modification::remove(const CaretPosition position) {
    try {
        if (position.character == 0 && position.line == 0) {
            return false;
        }
        const auto charactorOffset = _lineOffsets.at(position.line) + position.character;
        _content.erase(charactorOffset - 1, 1);
        for (auto& offset: _lineOffsets | views::drop(position.line + 1)) {
            offset -= 1;
        }
        if (position.character == 0) {
            // Delete new line
            _lineOffsets.erase(_lineOffsets.begin() + static_cast<int>(position.line));
        }
    } catch (out_of_range&) {
        return false;
    }
    _syncContent();
    return true;
}

void Modification::_syncContent() {
    thread([content = _content, path=path] {
        nlohmann::json requestBody = {
            {"content", encode(content, crypto::Encoding::Base64)},
            {"path", encode(path, crypto::Encoding::Base64)}
        };
        try {
            if (const auto [status, responseBody] = HttpHelper(
                    "http://localhost:3001",
                    chrono::seconds(10)
                ).post("/modify", move(requestBody));
                status != 200) {
                logger::log(format("(/modify) HTTP Code: {}, body: {}", status, responseBody.dump()));
            }
        } catch (HttpHelper::HttpError& e) {
            logger::error(format("(/modify) Http error: {}", e.what()));
        }
        catch (exception& e) {
            logger::log(format("(/modify) Exception: {}", e.what()));
        }
        catch (...) {
            logger::log("(/modify) Unknown exception.");
        }
    }).detach();
}

// bool Modification::merge(const Modification&other) {
//     switch (other.type) {
//         case Type::Addition: {
//             if (other.startPosition<startPosition || other.startPosition> endPosition + CaretPosition{1, 0}
//             ) {
//                 return false;
//             }
//             const auto insertPosition = other.startPosition - startPosition;
//             if (other._content.size() == 1) {
//                 _content[insertPosition.line].insert(
//                     insertPosition.character,
//                     other._content.front()
//                 );
//             }
//             else {
//                 _content[insertPosition.line].insert(
//                     insertPosition.character,
//                     other._content.front()
//                 );
//                 _content[insertPosition.line].append(
//                     other._content.back()
//                 );
//                 _content.insert(
//                     _content.begin() + static_cast<int>(insertPosition.line) + 1,
//                     other._content.begin() + 1,
//                     other._content.end() - 1
//                 );
//             }
//             _updateEndPositionWithContent();
//             break;
//         }
//         case Type::Deletion: {
//             if (other.startPosition<startPosition || other.startPosition> endPosition + CaretPosition{1, 0}
//             ) {
//                 return false;
//             }
//             // Erase other's range from current content
//             const auto eraseStartPosition = other.startPosition - startPosition;
//             if (const auto eraseEndPosition = other.endPosition - startPosition;
//                 eraseStartPosition.line == eraseEndPosition.line) {
//                 _content[eraseStartPosition.line].erase(
//                     eraseEndPosition.character,
//                     eraseEndPosition.character - eraseStartPosition.character
//                 );
//             }
//             else {
//                 _content[eraseEndPosition.line] =
//                         _content[eraseEndPosition.line].substr(0, eraseEndPosition.character) +
//                         _content[eraseStartPosition.line].erase(0, eraseStartPosition.character);
//                 _content.erase(
//                     _content.begin() + static_cast<int>(eraseEndPosition.line) + 1,
//                     _content.begin() + static_cast<int>(eraseStartPosition.line)
//                 );
//             }
//             _updateEndPositionWithContent();
//             break;
//         }
//         case Type::Swap: {
//             if (other.startPosition.line<startPosition.line || other.startPosition.line>
//                 endPosition.line ||
//                         other.endPosition.line<startPosition.line || other.startPosition.line>
//             endPosition.line
//             ) {
//                 return false;
//             }
//             // Swap lines
//             swap(_content[other.startPosition.line], _content[other.endPosition.line]);
//             break;
//         }
//         default: {
//             return false;
//         }
//     }
//     return true;
// }

// nlohmann::json Modification::parse() const {
//     nlohmann::json result = {
//         {
//             "endPosition", {
//                 {"character", endPosition.character},
//                 {"line", endPosition.line},
//             }
//         },
//         {
//             "startPosition",
//             {
//                 {"character", startPosition.character},
//                 {"line", startPosition.line},
//             }
//         },
//         {"type", magic_enum::enum_name(type)},
//     };
//     if (type == Type::Addition) {
//         result["content"] = _content;
//     }
//     return result;
// }
