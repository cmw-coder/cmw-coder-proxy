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

void Modification::add(const char character) {
    _buffer.push_back(character);
}

bool Modification::flush() {
    try {
        if (_removeCount > 0 || !_buffer.empty()) {
            CaretPosition newPosition = _lastPosition;
            if (_removeCount > 0) {
                if (_removeCount <= newPosition.character) {
                    _content.at(newPosition.line).erase(newPosition.character - _removeCount, _removeCount);
                    newPosition.addCharactor(-_removeCount);
                } else {
                    _content.at(newPosition.line).erase(0, newPosition.character);
                    auto toBeRemoved = _removeCount - newPosition.character;
                    newPosition.character = 0;
                    if (_lastPosition.line > 0) {
                        for (auto it = _content.begin() + static_cast<int>(_lastPosition.line) - 1;
                             it != _content.begin() && toBeRemoved > 0; --it) {
                            if (it->length() < toBeRemoved) {
                                // Plus 1 for '\n'
                                toBeRemoved -= it->length() + 1;
                                it = _content.erase(it);
                                newPosition.addLine(-1);
                            } else {
                                newPosition.character = it->length() - toBeRemoved;
                                it->erase(newPosition.character);
                                toBeRemoved = 0;
                            }
                        }
                    }
                }
            }
            if (!_buffer.empty()) {
                // Insert bufferLines into _content at newPosition
                if (const auto bufferLines =
                            views::split(_buffer, "\n"sv)
                            | views::transform([](auto&& r) { return string(&*r.begin(), ranges::distance(r)); })
                            | ranges::to<vector>();
                    bufferLines.size() > 1) {
                    const auto insertPoint = _content.begin() + static_cast<int>(newPosition.line);
                    *insertPoint = insertPoint->substr(0, newPosition.character).append(bufferLines.front());
                    newPosition.addCharactor(bufferLines.front().length());
                    if (newPosition.line == _content.size()) {
                        _content.insert(_content.end(), bufferLines.begin() + 1, bufferLines.end());
                        newPosition.addLine(bufferLines.size());
                        newPosition.character = bufferLines.back().length();
                    } else {
                        (insertPoint + 1)->insert(0, bufferLines.back());
                        if (bufferLines.size() > 2) {
                            _content.insert(
                                insertPoint,
                                bufferLines.begin() + 1,
                                bufferLines.end() - 1
                            );
                        }
                        newPosition.addLine(bufferLines.size() - 1);
                        newPosition.character = bufferLines.back().length();
                    }
                } else {
                    _content.at(newPosition.line).insert(newPosition.character, bufferLines.front());
                    newPosition.addCharactor(bufferLines.front().length());
                }
            }
            _lastPosition = newPosition;
        }
    } catch (out_of_range& e) {
        logger::warn(format("Out of range: {}", e.what()));
        return false;
    }
    return true;
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
            flush();
            if (_lastPosition.character + _buffer.length() > 0) {
            } else if (_lastPosition.line > 0) {
                flush({_lastPosition.line - 1, _content[_lastPosition.line - 1].length()});
            }
            break;
        }
        case Key::Up: {
            if (_lastPosition.line > 0) {
                flush({_lastPosition.line - 1, _lastPosition.character});
            }
            break;
        }
        case Key::Right: {
            if (_content.)
                break;
        }
        case Key::Down: {
            break;
        }
        default: {
            break;
        }
    }
}

void Modification::reload() {
    logger::debug(format("Reloading with path: '{}'", path));
    const auto content = fs::readFile(path);
    _syncContent();
    // Split content by '\n' or '\r\n' then store them in _content
    const sregex_token_iterator it(content.begin(), content.end(), regex(R"(\r?\n)"), -1);
    _content = {sregex_token_iterator(), {}};
}

void Modification::remove() {
    if (_content.empty()) {
        _removeCount++;
    } else {
        _content.pop_back();
    }
}

void Modification::_syncContent() {
    thread([content = accumulate(
        _content.begin(),
        _content.end(),
        string{}, [](auto& a, auto& b) { return a + b + '\n'; }
    ), path=path] {
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
