#include <magic_enum.hpp>
#include <ranges>
#include <regex>
#include <utility>

#include <helpers/HttpHelper.h>
#include <types/Modification.h>
#include <utils/crypto.h>
#include <utils/fs.h>
#include <utils/logger.h>

using namespace helpers;
using namespace std;
using namespace types;
using namespace utils;

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
    } else {
        _lastPosition.addCharactor(1);
    }
    _syncContent();
}

void Modification::navigate(const CaretPosition newPosition) {
    _lastPosition = newPosition;
}

void Modification::navigate(const Key key) {
    switch (key) {
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
                    _lastPosition.character = lineLength;
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
