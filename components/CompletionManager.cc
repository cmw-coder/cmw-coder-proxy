#include <chrono>
#include <format>

#include <magic_enum.hpp>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include <components/CompletionManager.h>
#include <components/Configurator.h>
#include <components/InteractionMonitor.h>
#include <components/ModificationManager.h>
#include <components/WebsocketManager.h>
#include <types/CaretPosition.h>
#include <utils/crypto.h>
#include <utils/logger.h>
#include <utils/system.h>

#include "WindowManager.h"

using namespace components;
using namespace helpers;
using namespace magic_enum;
using namespace std;
using namespace types;
using namespace utils;

namespace {
    constexpr auto completionGeneratedKey = "CMWCODER_completionGenerated";
    const vector<string> keywords = {"class", "if", "for", "struct", "switch", "union", "while"};

    bool checkNeedRetrieveCompletion(const char character) {
        const auto interactionMonitor = InteractionMonitor::GetInstance();
        const auto currentCaretPosition = interactionMonitor->getCaretPosition();
        const auto currentLineContent = interactionMonitor->getLineContent(currentCaretPosition.line);
        if (currentCaretPosition.character < currentLineContent.size()) {
            return false;
        }
        switch (character) {
            case ';': {
                if (ranges::none_of(keywords, [&currentLineContent](const string& keyword) {
                    return regex_match(currentLineContent, regex(format(R"(.*?\b{}\b.*?)", keyword)));
                })) {
                    logger::info("Normal input. Ignore due to ';' without any keyword");
                    return false;
                }
                return true;
            }
            case '}': {
                logger::info("Normal input. Ignore due to '}'");
                return false;
            }
            // TODO: Support more cases
            default: {
                return true;
            }
        }
    }
}

CompletionManager::CompletionManager() {
    _threadDebounceRetrieveCompletion();
}

void CompletionManager::interactionAcceptCompletion(const any&, bool& needBlockMessage) {
    // _isContinuousEnter.store(false);
    _isJustAccepted.store(true);
    string content;
    int64_t index; {
        unique_lock lock(_completionCacheMutex);
        tie(content, index) = _completionCache.reset();
    }
    if (!content.empty()) {
        const auto interactionMonitor = InteractionMonitor::GetInstance();
        const auto currentLineIndex = interactionMonitor->getCaretPosition().line;
        uint32_t insertedlineCount = 0;
        for (const auto lineRange: content.substr(index) | views::split("\r\n"sv)) {
            if (insertedlineCount == 0) {
                interactionMonitor->setSelectedContent({lineRange.begin(), lineRange.end()});
            } else {
                interactionMonitor->insertLineContent(
                    currentLineIndex + insertedlineCount, {lineRange.begin(), lineRange.end()}
                );
            }
            ++insertedlineCount;
        }
        WindowManager::GetInstance()->sendLeftThenRight();
        // system::setEnvironmentVariable(
        //     completionGeneratedKey,
        //     interactionMonitor->getLineContent(currentLineIndex) + regex_replace(content, regex("\r\n"), R"(\r\n)")
        // );
        // WindowManager::GetInstance()->sendAcceptCompletion();
        WebsocketManager::GetInstance()->sendAction(WsAction::CompletionAccept);
        logger::log(format("Accepted completion: {}", content));
        needBlockMessage = true;
    }
}

void CompletionManager::interactionDeleteInput(const any&, bool&) {
    // _isContinuousEnter.store(false);
    _isJustAccepted.store(false);
    uint32_t chatacter; {
        shared_lock lock(_componentsMutex);
        chatacter = _components.caretPosition.character;
    }
    try {
        if (chatacter != 0) {
            optional<pair<char, optional<string>>> previousCacheOpt; {
                unique_lock lock(_completionCacheMutex);
                previousCacheOpt = _completionCache.previous();
            }
            if (previousCacheOpt.has_value()) {
                // Has valid cache
                if (const auto [_, completionOpt] = previousCacheOpt.value();
                    completionOpt.has_value()) {
                    WebsocketManager::GetInstance()->sendAction(WsAction::CompletionCache, true);
                    logger::log("Delete backward. Send CompletionCache due to cache hit");
                    return;
                }
                _cancelCompletion();
                logger::log("Delete backward. Send CompletionCancel due to cache miss");
            }
        } else {
            bool hasValidCache; {
                shared_lock lock(_completionCacheMutex);
                hasValidCache = _completionCache.valid();
            }
            if (hasValidCache) {
                _isNewLine = true;
                _cancelCompletion();
                logger::log("Delete backward. Send CompletionCancel due to delete across line");
            }
        }
        _debounceRetrieveCompletionTime.store(chrono::high_resolution_clock::now());
    } catch (const bad_any_cast& e) {
        logger::log(format("Invalid delayedDelete data: {}", e.what()));
    }
}

void CompletionManager::interactionEnterInput(const any&, bool&) {
    _isJustAccepted.store(false);
    _isNewLine = true;
    // TODO: Support 1st level cache
    bool hasValidCache; {
        shared_lock lock(_completionCacheMutex);
        hasValidCache = _completionCache.valid();
    }
    if (hasValidCache) {
        _cancelCompletion();
        logger::log("Enter Input. Send CompletionCancel");
    }
    _debounceRetrieveCompletionTime.store(chrono::high_resolution_clock::now());
    _needRetrieveCompletion.store(true);
    // if (_isContinuousEnter.load()) {
    //     _debounceRetrieveCompletionTime.store(chrono::high_resolution_clock::now());
    //     _needRetrieveCompletion.store(true);
    //     logger::log("Detect Continuous enter, retrieve use previous completion");
    // } else {
    //     _isContinuousEnter.store(true);
    //     _debounceRetrieveCompletionTime.store(chrono::high_resolution_clock::now());
    //     _needRetrieveCompletion.store(true);
    //     logger::log("Detect first enter, retrieve editor info first");
    // }
}

void CompletionManager::interactionNavigateWithKey(const any& data, bool&) {
    // _isContinuousEnter.store(false);
    try {
        switch (any_cast<Key>(data)) {
            case Key::PageDown:
            case Key::PageUp:
            case Key::Left:
            case Key::Up:
            case Key::Right:
            case Key::Down: {
                _isNewLine = true;
            }
            default: {
                break;
            }
        }
        _cancelCompletion();
        logger::log("Navigate. Send CompletionCancel");
    } catch (const bad_any_cast& e) {
        logger::log(format("Invalid interactionNavigate data: {}", e.what()));
    }
}

void CompletionManager::interactionNavigateWithMouse(const any& data, bool&) {
    // _isContinuousEnter.store(false);
    try {
        const auto [newCursorPosition, _] = any_cast<tuple<CaretPosition, CaretPosition>>(data); {
            shared_lock lock(_componentsMutex);
            if (_components.caretPosition.line != newCursorPosition.line) {
                _isNewLine = true;
            }
            if (_components.caretPosition != newCursorPosition) {
                _cancelCompletion();
            }
        }
        unique_lock lock(_componentsMutex);
        _components.caretPosition = newCursorPosition;
    } catch (const bad_any_cast& e) {
        logger::log(format("Invalid interactionCaretUpdate data: {}", e.what()));
    }
}

void CompletionManager::interactionNormalInput(const any& data, bool&) {
    try {
        bool needRetrieveCompletion = false;
        const auto character = any_cast<char>(data);
        // _isContinuousEnter.store(false);
        _isJustAccepted.store(false);
        optional<pair<char, optional<string>>> nextCacheOpt; {
            unique_lock lock(_completionCacheMutex);
            nextCacheOpt = _completionCache.next();
        }
        if (nextCacheOpt.has_value()) {
            // Has valid cache
            if (const auto [currentChar, completionOpt] = nextCacheOpt.value();
                character == currentChar) {
                // Cache hit
                if (completionOpt.has_value()) {
                    // In cache
                    WebsocketManager::GetInstance()->sendAction(WsAction::CompletionCache, false);
                    logger::log("Normal input. Send CompletionCache due to cache hit");
                } else {
                    // Out of cache
                    {
                        unique_lock lock(_completionCacheMutex);
                        _completionCache.reset();
                    }
                    WebsocketManager::GetInstance()->sendAction(WsAction::CompletionAccept);
                    logger::log("Normal input. Send CompletionAccept due to cache complete");
                }
            } else {
                // Cache miss
                _cancelCompletion();
                logger::log("Normal input. Send CompletionCancel due to cache miss");
                needRetrieveCompletion = true;
            }
        } else {
            // No valid cache
            needRetrieveCompletion = true;
        }
        if (needRetrieveCompletion) {
            _needRetrieveCompletion.store(checkNeedRetrieveCompletion(character));
            _debounceRetrieveCompletionTime.store(chrono::high_resolution_clock::now());
        }
    } catch (const bad_any_cast& e) {
        logger::warn(format("Invalid interactionNormalInput data: {}", e.what()));
    } catch (const runtime_error& e) {
        logger::warn(e.what());
    }
}

void CompletionManager::interactionSave(const any&, bool&) {
    bool hasValidCache; {
        shared_lock lock(_completionCacheMutex);
        hasValidCache = _completionCache.valid();
    }
    if (hasValidCache) {
        _cancelCompletion();
        logger::log("Save. Send CompletionCancel");
    }
}

void CompletionManager::instantUndo(const any&) {
    // _isContinuousEnter.store(false);
    _isNewLine = true;
    if (_isJustAccepted.load()) {
        _isJustAccepted.store(false);
    } else {
        bool hasValidCache; {
            shared_lock lock(_completionCacheMutex);
            hasValidCache = _completionCache.valid();
        }
        if (hasValidCache) {
            _cancelCompletion();
            logger::log("Undo. Send CompletionCancel");
        } else {
            // Invalidate current retrieval
            _debounceRetrieveCompletionTime.store(chrono::high_resolution_clock::now());
            _needRetrieveCompletion.store(false);
        }
    }
}

void CompletionManager::wsActionCompletionGenerate(const nlohmann::json& data) {
    if (_debounceRetrieveCompletionTime.load() > _wsActionSentTime.load()) {
        logger::log("(WsAction::CompletionGenerate) Ignore due to debounce");
        return;
    }
    if (const auto result = data["result"].get<string>();
        result == "success") {
        if (const auto& completions = data["completions"];
            completions.is_array() && !completions.empty()) {
            // TODO: Support multiple completions
            {
                unique_lock lock(_completionCacheMutex);
                _completionCache.reset(decode(completions[0].get<string>(), crypto::Encoding::Base64));
            }
        } else {
            logger::log("(WsAction::CompletionGenerate) No completion");
        }
    } else {
        logger::warn(format(
            "(WsAction::CompletionGenerate) Result: {}\n"
            "\tMessage: {}",
            result,
            data["message"].get<string>()
        ));
    }
}

void CompletionManager::setAutoCompletion(const bool isAutoCompletion) {
    if (isAutoCompletion != _isAutoCompletion.load()) {
        _isAutoCompletion.store(isAutoCompletion);
        logger::log(format("Auto completion switch {}", _isAutoCompletion.load() ? "on" : "off"));
    }
}

void CompletionManager::setProjectId(const string& projectId) {
    bool needSet; {
        shared_lock lock(_editorInfoMutex);
        needSet = _editorInfo.projectId != projectId;
    }
    if (needSet) {
        unique_lock lock(_editorInfoMutex);
        _editorInfo.projectId = projectId;
    }
}

void CompletionManager::setVersion(const string& version) {
    bool needSet; {
        shared_lock lock(_editorInfoMutex);
        needSet = _editorInfo.version.empty();
    }
    if (needSet) {
        unique_lock lock(_editorInfoMutex);
        _editorInfo.version = Configurator::GetInstance()->reportVersion(version);
        logger::log(format("Plugin version: {}", version));
    }
}

void CompletionManager::_cancelCompletion() {
    WebsocketManager::GetInstance()->sendAction(WsAction::CompletionCancel);
    unique_lock lock(_completionCacheMutex);
    _completionCache.reset();
}

void CompletionManager::_sendCompletionGenerate() {
    try {
        shared_lock componentsLock(_componentsMutex);
        shared_lock editorInfoLock(_editorInfoMutex);
        const auto [xPixel, yPixel] = InteractionMonitor::GetInstance()->getCaretPixels(
            _components.caretPosition.line
        );
        _wsActionSentTime.store(chrono::high_resolution_clock::now());
        WebsocketManager::GetInstance()->sendAction(
            WsAction::CompletionGenerate,
            {
                {
                    "caret", {
                        {"character", _components.caretPosition.character},
                        {"line", _components.caretPosition.line},
                        {"xPixel", xPixel},
                        {"yPixel", yPixel},
                    }
                },
                {"path", encode(_components.path, crypto::Encoding::Base64)},
                {"prefix", encode(_components.prefix, crypto::Encoding::Base64)},
                {"projectId", _editorInfo.projectId},
                {"suffix", encode(_components.suffix, crypto::Encoding::Base64)},
                {"symbolString", encode(_components.symbolString, crypto::Encoding::Base64)},
                {"tabString", encode(_components.tabString, crypto::Encoding::Base64)},
            }
        );
    } catch (const runtime_error& e) {
        logger::warn(e.what());
    }
}

void CompletionManager::_threadDebounceRetrieveCompletion() {
    thread([this] {
        while (_isRunning) {
            if (_needRetrieveCompletion.load()) {
                if (const auto pastTime = chrono::high_resolution_clock::now() - _debounceRetrieveCompletionTime.load();
                    pastTime >= chrono::milliseconds(500)) {
                    try {
                        const auto currentCaretPosition = InteractionMonitor::GetInstance()->getCaretPosition();
                        const auto getContextLine = [&](const int offset = 0) {
                            return InteractionMonitor::GetInstance()->
                                    getLineContent(currentCaretPosition.line + offset);
                        };
                        const auto currentLine = getContextLine();
                        if (currentLine.length() < currentCaretPosition.character) {
                            logger::warn(format(
                                "Read current line failed\n"
                                "\t CurrentCaret: ({}, {})\n"
                                "\tContent: {} (Length: {}))",
                                currentCaretPosition.line,
                                currentCaretPosition.character,
                                currentLine,
                                currentLine.length()
                            ));
                            this_thread::sleep_for(chrono::milliseconds(25));
                            continue;
                        }
                        auto prefix = currentLine.substr(0, currentCaretPosition.character);
                        auto suffix = currentLine.substr(currentCaretPosition.character);
                        if (_isNewLine) {
                            for (auto index = 1; index <= 30; ++index) {
                                prefix.insert(0, getContextLine(-index).append("\r\n"));
                            }
                            for (auto index = 1; index <= 5; ++index) {
                                suffix.append("\r\n").append(getContextLine(index));
                            } {
                                unique_lock lock(_componentsMutex);
                                _components.caretPosition = currentCaretPosition;
                                _components.path = InteractionMonitor::GetInstance()->getFileName();
                                _components.prefix = move(prefix);
                                _components.suffix = move(suffix);
                            }

                            _isNewLine = false;
                        } else {
                            unique_lock lock(_componentsMutex);
                            _components.caretPosition = currentCaretPosition;
                            _components.path = InteractionMonitor::GetInstance()->getFileName();
                            if (const auto lastNewLineIndex = _components.prefix.find_last_of('\n');
                                lastNewLineIndex != string::npos) {
                                _components.prefix = _components.prefix.substr(0, lastNewLineIndex).append(prefix);
                            } else {
                                _components.prefix = prefix;
                            }
                            if (const auto firstNewLineIndex = suffix.find_first_of('\n');
                                firstNewLineIndex != string::npos) {
                                _components.suffix = _components.suffix.substr(firstNewLineIndex + 1).insert(0, suffix);
                            } else {
                                _components.suffix = suffix;
                            }
                        }
                        _sendCompletionGenerate();
                        _needRetrieveCompletion.store(false);
                    } catch (const runtime_error& e) {
                        logger::warn(e.what());
                        this_thread::sleep_for(chrono::milliseconds(10));
                    }
                }
            } else {
                this_thread::sleep_for(chrono::milliseconds(10));
            }
        }
    }).detach();
}
