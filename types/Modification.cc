#include <magic_enum.hpp>
#include <ranges>
#include <regex>
#include <utility>
#include <algorithm>

#include <helpers/HttpHelper.h>
#include <types/Modification.h>
#include <utils/crypto.h>
#include <utils/fs.h>
#include <utils/logger.h>

using namespace helpers;
using namespace std;
using namespace types;
using namespace utils;

#define TAB  "    "

Modification::Modification(string path): path(move(path)), _wsHelper("ws://127.0.0.1:3000") {
    reload();
}

void Modification::add(const char character) {
    const auto charactorOffset = _lineOffsets.at(_lastPosition.line) + _lastPosition.character;
    for (auto& offset: _lineOffsets | views::drop(_lastPosition.line + 1)) {
        offset += 1;
    }
    _content.insert(charactorOffset, 1, character);
    if (character == '\n') {
        // Insert new line
        _lineOffsets.insert(
            _lineOffsets.begin() + static_cast<int>(_lastPosition.line) + 1,
            charactorOffset + 1
        );
        _lastPosition.addLine(1);
        _lastPosition.character = 0;
        _lastPosition.maxCharacter = 0;
    } else {
        _lastPosition.addCharactor(1);
    }
    _syncContent();
}

void Modification::add(const string& characters) {
    logger::info(format("add string line {} char {}", _lastPosition.line, _lastPosition.character));
    const auto charactorOffset = _lineOffsets.at(_lastPosition.line) + _lastPosition.character;
    for (auto& offset : _lineOffsets | views::drop(_lastPosition.line + 1)) {
        offset += characters.length();
    }
    _content.insert(charactorOffset, characters);
    if (const auto lineCount = count(characters.begin(), characters.end(), '\n');
        lineCount > 0) {
        int index = 0;
        int preindex = 0;
        for (int count = 0; count < lineCount; count++) {
            index = characters.find('\n', preindex);
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

void Modification::navigate(const CaretPosition newPosition) {
    _lastPosition = newPosition;
}

void Modification::navigate(const Key key) {
    switch (key) {
        case Key::Tab: {
            add(TAB);
            break;
        }
        case Key::Home: {
            break;
        }
        case Key::End: {
            break;
        }
        case Key::PageDown: {
            break;
        }
        case Key::PageUp: {
            break;
        }
        case Key::Left: {
            if (_lastPosition.character > 0) {
                _lastPosition.addCharactor(-1);
            } else if (_lastPosition.line > 0) {
                _lastPosition.character = _getLineLength(_lastPosition.line - 1);
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
    return _lastSelect.isEmpty();
}

std::string Modification::getText(Range range) {
    const auto [startOffset, endOffset] = _rangeToCharactorOffset(range);
    auto content = _content.substr(startOffset, endOffset - startOffset);
    return content;
}

void Modification::replace(const std::string& characters) {
    remove(_lastSelect);
    add(characters);
}

void Modification::remove(Range range) {
    const auto [startOffset, endOffset] = _rangeToCharactorOffset(range);
    const auto subContent = getText(range);
    const auto subLength = endOffset - startOffset;
    _content.erase(startOffset, endOffset - startOffset);
    const auto enterCount = count(subContent.begin(), subContent.end(), '\n');

    for (auto it = (_lineOffsets.begin() + static_cast<int>(range.start.line) + 1);
         it != _lineOffsets.end(); it++) {
        if (distance(_lineOffsets.begin(), it) <= subLength) {
            _lineOffsets.erase(it);
        } else {
            *it -=subContent.length();
        }
    }
    _lastPosition = range.start;
    _lastPosition.maxCharacter = _lastPosition.character;
    _syncContent();
}

void Modification::selectRemove() {
    remove(_lastSelect);
}

void Modification::_syncContent() {
    thread([this, content = _content, path=path] {
        _wsHelper.sendAction(
            WsHelper::Action::Sync,
            {
                {"content", encode(content, crypto::Encoding::Base64)},
                {"path", encode(path, crypto::Encoding::Base64)}
            }
        );
        // try {
        //     if (const auto [status, responseBody] = HttpHelper(
        //             "http://localhost:3001",
        //             chrono::seconds(10)
        //         ).post("/modify", move(requestBody));
        //         status != 200) {
        //         logger::log(format("(/modify) HTTP Code: {}, body: {}", status, responseBody.dump()));
        //     }
        // } catch (HttpHelper::HttpError& e) {
        //     logger::error(format("(/modify) Http error: {}", e.what()));
        // }
        // catch (exception& e) {
        //     logger::log(format("(/modify) Exception: {}", e.what()));
        // }
        // catch (...) {
        //     logger::log("(/modify) Unknown exception.");
        // }
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
    uint32_t endCharactorOffset = _lineOffsets.at(range.end.line) + range.end.character;
    return make_pair(startCharactorOffset, endCharactorOffset);
}
