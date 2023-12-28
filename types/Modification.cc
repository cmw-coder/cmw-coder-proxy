/**
 * @file Modification.cc
 * @brief Contains the implementation of the Modification class which is used to modify a file's content.
 */

#include <magic_enum.hpp>
#include <ranges>
#include <regex>
#include <utility>
#include <algorithm>

#include <helpers/HttpHelper.h>
#include <types/IndentType.h>
#include <types/Modification.h>
#include <utils/crypto.h>
#include <utils/fs.h>
#include <utils/logger.h>

using namespace helpers;
using namespace std;
using namespace types;
using namespace utils;

namespace {
    /**
     * @brief A constant string representing a tab.
     */
    constexpr auto tabString = "    ";
}

/**
 * @brief Constructs a new Modification object.
 * @param path The path to the file to be recorded.
 */
Modification::Modification(string path): path(move(path)), _wsHelper("ws://127.0.0.1:3000") {
    reload();
}

/**
 * @brief Adds a character to the content at the current position.
 * @param character The character to be added.
 */
void Modification::add(const char character) {
    logger::info(format("Add character at ({}, {})", _lastPosition.line, _lastPosition.character));
    // TODO: Get indent type from config
    constexpr auto indentType = IndentType::Simple;
    const auto charactorOffset = _lineOffsets.at(_lastPosition.line) + _lastPosition.character;
    if (character == '\n') {
        // Insert new line
        const auto currentLineIndent = _getLineIndent(_lastPosition.line);

        _lineOffsets.insert(
            _lineOffsets.begin() + static_cast<int>(_lastPosition.line) + 1,
            charactorOffset + 1
        );
        _lastPosition.addLine(1);

        switch (indentType) {
            case IndentType::None: {
                _lastPosition.character = 0;
                _lastPosition.maxCharacter = 0;
                _content.insert(charactorOffset, 1, character);
                for (auto& offset: _lineOffsets | views::drop(_lastPosition.line + 1)) {
                    offset += 1;
                }
                break;
            }
            case IndentType::Simple: {
                // Insert new line and indent
                const auto insertContent = string(1, character).append(string(currentLineIndent, ' '));
                _lastPosition.character = currentLineIndent;
                _lastPosition.maxCharacter = currentLineIndent;
                _content.insert(charactorOffset, insertContent);
                for (auto& offset: _lineOffsets | views::drop(_lastPosition.line + 1)) {
                    offset += insertContent.length();
                }
                logger::debug(_content);
                // Remove extra indent of next line
                // _lastPosition.line was increased by 1 in the previous codes
                if (const auto nextLineIndent = _getLineIndent(_lastPosition.line);
                    nextLineIndent > currentLineIndent) {
                    _content.erase(_lineOffsets.at(_lastPosition.line) + currentLineIndent, nextLineIndent - currentLineIndent);
                    for (auto& offset: _lineOffsets | views::drop(_lastPosition.line + 1)) {
                        offset -= nextLineIndent - currentLineIndent;
                    }
                }
                break;
            }
            case IndentType::Smart: {
                // TODO: Implement smart indent
                break;
            }
        }
    } else {
        _lastPosition.addCharactor(1);
        _content.insert(charactorOffset, 1, character);
        for (auto& offset: _lineOffsets | views::drop(_lastPosition.line + 1)) {
            offset += 1;
        }
    }
    _syncContent();
}

/**
 * @brief Adds a string of characters to the content at the current position.
 * @param characters The string of characters to be added.
 */
void Modification::add(string characters) {
    logger::info(format("Add string at ({}, {})", _lastPosition.line, _lastPosition.character));
    const auto charactorOffset = _lineOffsets.at(_lastPosition.line) + _lastPosition.character;
    for (auto& offset: _lineOffsets | views::drop(_lastPosition.line + 1)) {
        offset += characters.length();
    }
    _content.insert(charactorOffset, characters);
    if (const auto lineCount = ranges::count(characters, '\n');
        lineCount > 0) {
        uint64_t preindex = 0;
        for (auto count = 0; count < lineCount; count++) {
            const auto index = characters.find('\n', preindex);
            _lineOffsets.insert(
                _lineOffsets.begin() + static_cast<int>(_lastPosition.line) + 1,
                index - preindex + 1
            );
            preindex = index + 1;
        }
        _lastPosition.addLine(lineCount);
    }
    _lastPosition.addCharactor(characters.length());
    _syncContent();
}

/**
 * @brief Navigates to a new position in the content.
 * @param newPosition The new position to navigate to.
 */
void Modification::navigate(const CaretPosition& newPosition) {
    _lastPosition = newPosition;
}

/**
 * @brief Navigates in the content based on a specified key.
 * @param key The key that determines how to navigate.
 */
void Modification::navigate(const Key key) {
    switch (key) {
        case Key::Tab: {
            if (isSelect()) {
                const auto selectContent = _getSelectTabContent(_lastSelect);
                replace(_lastSelect, selectContent);
            } else {
                add(TAB);
            }
            break;
        }
        // TODO: Implement these keys
        // case Key::Home: {
        //     break;
        // }
        // case Key::End: {
        //     break;
        // }
        // case Key::PageDown: {
        //     break;
        // }
        // case Key::PageUp: {
        //     break;
        // }
        case Key::Left: {
            if (_lastPosition.character > 0) {
                _lastPosition.addCharactor(-1);
            } else if (_lastPosition.line > 0) {
                _lastPosition.character = _getLineLength(_lastPosition.line - 1);
                _lastPosition.maxCharacter = _lastPosition.character;
                _lastPosition.addLine(-1);
            }

            break;
        }
        case Key::Up: {
            if (_lastPosition.line > 0) {
                _lastPosition.addLine(-1);
                if (const auto lineLength = _getLineLength(_lastPosition.line);
                    lineLength < _lastPosition.character) {
                    _lastPosition.character = lineLength;
                    _lastPosition.character = min(lineLength, _lastPosition.maxCharacter);
                }
            }
            break;
        }
        case Key::Right: {
            if (const auto lineLength = _getLineLength(_lastPosition.line);
                _lastPosition.character == lineLength) {
                if (_lastPosition.line < _lineOffsets.size() - 1) {
                    _lastPosition.addLine(1);
                    _lastPosition.character = 0;
                    _lastPosition.maxCharacter = 0;
                }
            } else {
                _lastPosition.addCharactor(1);
            }

            break;
        }
        case Key::Down: {
            if (_lastPosition.line < _lineOffsets.size() - 1) {
                _lastPosition.addLine(1);
                if (const auto lineLength = _getLineLength(_lastPosition.line);
                    lineLength < _lastPosition.character) {
                    _lastPosition.character = min(lineLength, _lastPosition.maxCharacter);
                }
            }
            break;
        }
        default: {
            break;
        }
    }
}

/**
 * @brief Reloads the content from the file.
 */
void Modification::reload() {
    logger::debug(format("Reloading with path: '{}'", path));
    _content = fs::readFile(path);
    _lineOffsets.clear();
    _syncContent();
    // Split content by '\n' or '\r\n' then store them in _content
    _lineOffsets.push_back(0);
    for (const auto& line: _content | views::split('\n')) {
        _lineOffsets.push_back(_lineOffsets.back() + line.size() + 1);
    }
    _lineOffsets.pop_back();
}

/**
 * @brief Removes a character from the content at the current position.
 */
void Modification::remove() {
    if (_lastPosition.character == 0 && _lastPosition.line == 0) {
        return;
    }
    const auto charactorOffset = _lineOffsets.at(_lastPosition.line) + _lastPosition.character;
    _content.erase(charactorOffset - 1, 1);
    for (auto& offset: _lineOffsets | views::drop(_lastPosition.line + 1)) {
        offset -= 1;
    }
    if (_lastPosition.character == 0) {
        // Delete new line
        _lineOffsets.erase(_lineOffsets.begin() + static_cast<int>(_lastPosition.line));
        _lastPosition.addLine(-1);
        _lastPosition.character = _getLineLength(_lastPosition.line);
    } else {
        _lastPosition.addCharactor(-1);
    }
    _syncContent();
}

void Modification::select(const Range range) {
    _lastSelect = range;
}

void Modification::clearSelect() {
    _lastSelect = Range(0, 0, 0, 0);
}

bool Modification::isSelect() const {
    return !_lastSelect.isEmpty();
}

std::string Modification::getText(Range range) {
    const auto [startOffset, endOffset] = _rangeToCharactorOffset(range);
    auto content = _content.substr(startOffset, endOffset - startOffset);
    return content;
}

void Modification::replace(const std::string& characters) {
    const auto selectRange = _lastSelect;
    clearSelect();
    remove(selectRange);
    add(characters);
    _syncContent();
}

void Modification::replace(const Range selectRange, const std::string& characters) {
    remove(selectRange);
    add(characters);
    _syncContent();
}

void Modification::remove(const Range range) {
    const auto [startOffset, endOffset] = _rangeToCharactorOffset(range);
    const auto subContent = getText(range);
    const auto subLength = endOffset - startOffset;
    _content.erase(startOffset, subLength);
    const auto enterCount = count(subContent.begin(), subContent.end(), '\n');
    for (auto it = (_lineOffsets.begin() + static_cast<int>(range.start.line) + 1);
         it != _lineOffsets.end();) {
        if (distance(_lineOffsets.begin(), it) <= enterCount) {
            it = _lineOffsets.erase(it);
            continue;
        } else {
            *it -=subContent.length();
            it++;
        }
    }
    _lastPosition = range.start;
    _lastPosition.maxCharacter = _lastPosition.character;
    _syncContent();
}

void Modification::selectRemove() {
    remove(_lastSelect);
    clearSelect();
    _syncContent();
}

/**
 * @brief Gets the indentation of a line.
 * @param lineIndex The index of the line for which the indentation is to be obtained.
 * @return The indentation of the line.
 */
uint32_t Modification::_getLineIndent(const uint32_t lineIndex) const {
    const auto [start, end] = _getLineRange(lineIndex);
    for (auto index = start; index < end; index++) {
        if (_content.at(index) != ' ') {
            return index - start;
        }
    }
    return end - start;
}

/**
 * @brief Gets the length of a line.
 *
 * This function returns the length of a line, which is the number of characters in the line.
 *
 * @param lineIndex The index of the line for which the length is to be obtained.
 * @return The length of the line.
 */
uint32_t Modification::_getLineLength(const uint32_t lineIndex) const {
    const auto [start, end] = _getLineRange(lineIndex);
    return end - start;
}

/**
 * @brief Get the range of a line in the content.
 *
 * This function returns a pair of unsigned integers representing the start and end positions
 * of a line in the content. The line is specified by its index.
 *
 * @param lineIndex The index of the line for which the range is to be obtained.
 * @return A pair of unsigned integers where the first element is the start position and the second element is the end position of the line in the content.
 */
pair<uint32_t, uint32_t> Modification::_getLineRange(const uint32_t lineIndex) const {
    return {
        _lineOffsets.at(lineIndex),
        lineIndex == _lineOffsets.size() - 1
            ? _content.length()
            : _lineOffsets.at(lineIndex + 1) - 1
    };
}

void Modification::_syncContent() {
    thread([this, content = _content, path=path] {
        _wsHelper.sendAction(
            WsHelper::Action::DebugSync,
            {
                {"content", encode(content, crypto::Encoding::Base64)},
                {"path", encode(path, crypto::Encoding::Base64)}
            }
        );
    }).detach();
}

uint32_t Modification::_getLineLength(const uint32_t lineIndex) const {
    if (lineIndex == _lineOffsets.size() - 1) {
        return _content.length() - _lineOffsets.back();
    }
    return _lineOffsets.at(lineIndex + 1) - _lineOffsets.at(lineIndex) - 1;
}

pair<uint32_t, uint32_t> Modification::_rangeToCharactorOffset(Range range) const {
    uint32_t startCharactorOffset = _lineOffsets.at(range.start.line) + range.start.character;
    uint32_t endCharactorOffset;
    if (range.end.character == 4096) {
        endCharactorOffset =  _lineOffsets.at(range.end.line) + _getLineLength(range.end.line) + 1;
    } else {
        endCharactorOffset = _lineOffsets.at(range.end.line) + range.end.character;
    }

    return make_pair(startCharactorOffset, endCharactorOffset);
}

std::string Modification::_getSelectTabContent(const Range range) {
    string selectcontent = "";
    const auto Content = getText(range);
    if (const auto lineCount = count(Content.begin(), Content.end(), '\n');
        lineCount >= 0) {
        int index = 0;
        int preindex = 0;
        for (int count = 0; count <= lineCount; count++) {
            index = Content.find('\n', preindex);
            auto subContent = Content.substr(preindex, index-preindex);
            selectcontent += TAB + subContent;
            preindex = index + 1;
            if (count < lineCount) {
                selectcontent += "\n";
            }
        }
    }
    return selectcontent;
}
