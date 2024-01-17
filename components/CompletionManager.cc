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
#include <components/WindowManager.h>
#include <types/CaretPosition.h>
#include <utils/logger.h>
#include <utils/system.h>


using namespace components;
using namespace helpers;
using namespace magic_enum;
using namespace std;
using namespace types;
using namespace utils;

namespace {
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
            case '{': {
                logger::info("Normal input. Ignore due to '{'");
                return false;
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

CompletionManager::~CompletionManager() {
    _isRunning = false;
}

void CompletionManager::interactionCompletionAccept(const any&, bool& needBlockMessage) {
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
        uint32_t insertedlineCount{0}, lastLineLength{0};
        for (const auto lineRange: content.substr(index) | views::split("\r\n"sv)) {
            auto lineContent = string{lineRange.begin(), lineRange.end()};
            if (insertedlineCount == 0) {
                interactionMonitor->setSelectedContent(lineContent);
            } else {
                lastLineLength = lineContent.size();
                interactionMonitor->insertLineContent(
                    currentLineIndex + insertedlineCount, lineContent
                );
            }
            ++insertedlineCount;
        }
        WindowManager::GetInstance()->sendLeftThenRight();
        if (insertedlineCount > 1) {
            logger::debug(format("Goto: ({}, {})", currentLineIndex + insertedlineCount - 1, lastLineLength));
            interactionMonitor->setCaretPosition(
                {lastLineLength, currentLineIndex + insertedlineCount - 1}
            );
        }
        logger::log(format("Accepted completion: {}", content));
        WebsocketManager::GetInstance()->send(CompletionAcceptClientMessage(content));
        needBlockMessage = true;
    }
}

void CompletionManager::interactionCompletionCancel(const any& data, bool&) {
    _cancelCompletion();
    logger::log("Cancel completion, Send CompletionCancel");
    try {
        if (any_cast<bool>(data)) {
            _requestRetrieveCompletion();
            WindowManager::GetInstance()->sendLeftThenRight();
        }
    } catch (const bad_any_cast& e) {
        logger::log(format("Invalid interactionCompletionCancel data: {}", e.what()));
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
                    WebsocketManager::GetInstance()->send(CompletionCacheClientMessage(true));
                    logger::log("Delete backward. Send CompletionCache due to cache hit");
                } else {
                    _cancelCompletion();
                    logger::log("Delete backward. Send CompletionCancel due to cache miss");
                }
            }
        } else if (_hasValidCache()) {
            _isNewLine = true;
            _cancelCompletion();
            logger::log("Delete backward. Send CompletionCancel due to delete across line");
        }
    } catch (const bad_any_cast& e) {
        logger::log(format("Invalid delayedDelete data: {}", e.what()));
    }
}

void CompletionManager::interactionEnterInput(const any&, bool&) {
    _isJustAccepted.store(false);
    _isNewLine = true;
    // TODO: Support 1st level cache
    if (_hasValidCache()) {
        _cancelCompletion();
        logger::log("Enter Input. Send CompletionCancel");
    }
    _requestRetrieveCompletion();
    // if (_isContinuousEnter.load()) {
    //     _requestRetrieveCompletion();
    //     logger::log("Detect Continuous enter, retrieve use previous completion");
    // } else {
    //     _isContinuousEnter.store(true);
    //     _requestRetrieveCompletion();
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
        if (_hasValidCache()) {
            _cancelCompletion();
            logger::log("Navigate with key. Send CompletionCancel");
        }
    } catch (const bad_any_cast& e) {
        logger::log(format("Invalid interactionNavigateWithKey data: {}", e.what()));
    }
}

void CompletionManager::interactionNavigateWithMouse(const any& data, bool&) {
    // _isContinuousEnter.store(false);
    try {
        const auto [newCursorPosition, _] = any_cast<tuple<CaretPosition, CaretPosition>>(data); {
            shared_lock componentsLock(_componentsMutex);
            if (_components.caretPosition.line != newCursorPosition.line) {
                _isNewLine = true;
            }
            if (_components.caretPosition != newCursorPosition) {
                if (_hasValidCache()) {
                    _cancelCompletion();
                    logger::log("Navigate with mouse. Send CompletionCancel");
                }
            }
        }
        unique_lock lock(_componentsMutex);
        _components.caretPosition = newCursorPosition;
    } catch (const bad_any_cast& e) {
        logger::log(format("Invalid interactionNavigateWithMouse data: {}", e.what()));
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
                    WebsocketManager::GetInstance()->send(CompletionCacheClientMessage(false));
                    logger::log("Normal input. Send CompletionCache due to cache hit");
                } else {
                    // Out of cache
                    string content; {
                        unique_lock lock(_completionCacheMutex);
                        content = _completionCache.reset().first;
                    }
                    WebsocketManager::GetInstance()->send(CompletionAcceptClientMessage(content));
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
            _prolongRetrieveCompletion();
            _needDiscardWsAction.store(true);
            if (checkNeedRetrieveCompletion(character)) {
                _needRetrieveCompletion.store(true);
            }
        }
    } catch (const bad_any_cast& e) {
        logger::warn(format("Invalid interactionNormalInput data: {}", e.what()));
    } catch (const runtime_error& e) {
        logger::warn(e.what());
    }
}

void CompletionManager::interactionPaste(const any&, bool&) {
    if (_hasValidCache()) {
        _cancelCompletion();
        logger::log("Paste. Send CompletionCancel");
    }

    _isNewLine = true;
}

void CompletionManager::interactionSave(const any&, bool&) {
    if (_hasValidCache()) {
        _cancelCompletion();
        logger::log("Save. Send CompletionCancel");
    }
}

void CompletionManager::interactionUndo(const any&, bool&) {
    // _isContinuousEnter.store(false);
    _isNewLine = true;
    if (_isJustAccepted.load()) {
        _isJustAccepted.store(false);
        return;
    }

    if (_hasValidCache()) {
        _cancelCompletion();
        logger::log("Undo. Send CompletionCancel");
    } else {
        // Invalidate current retrieval
        _prolongRetrieveCompletion();
        _needDiscardWsAction.store(true);
        _needRetrieveCompletion.store(false);
    }
}

void CompletionManager::wsActionCompletionGenerate(const nlohmann::json& data) {
    if (_needDiscardWsAction.load()) {
        logger::log("(WsAction::CompletionGenerate) Ignore due to debounce");
        WebsocketManager::GetInstance()->send(CompletionCancelClientMessage());
        return;
    }
    if (const auto result = data["result"].get<string>();
        result == "success") {
        if (const auto& completions = data["completions"];
            completions.is_array() && !completions.empty()) {
            {
                unique_lock lock(_completionListMutex);
                _completionList = completions.get<vector<string>>();
            } {
                unique_lock completionCacheLock(_completionCacheMutex);
                _completionCache.reset(completions[0].get<string>());
            }
            const auto [xPos, yPos] = InteractionMonitor::GetInstance()->getCaretPixels(
                InteractionMonitor::GetInstance()->getCaretPosition().line
            );
            WebsocketManager::GetInstance()->send(
                CompletionSelectClientMessage(completions[0].get<string>(), 0, completions.size(), xPos, yPos)
            );
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

void CompletionManager::_cancelCompletion() {
    WebsocketManager::GetInstance()->send(CompletionCancelClientMessage());
    unique_lock lock(_completionCacheMutex);
    _completionCache.reset();
}

bool CompletionManager::_hasValidCache() const {
    bool hasValidCache; {
        shared_lock lock(_completionCacheMutex);
        hasValidCache = _completionCache.valid();
    }
    return hasValidCache;
}

void CompletionManager::_prolongRetrieveCompletion() {
    _debounceRetrieveCompletionTime.store(chrono::high_resolution_clock::now());
}

void CompletionManager::_requestRetrieveCompletion() {
    _prolongRetrieveCompletion();
    _needDiscardWsAction.store(true);
    _needRetrieveCompletion.store(true);
}

void CompletionManager::_sendCompletionGenerate() {
    try {
        shared_lock componentsLock(_componentsMutex);
        _needDiscardWsAction.store(false);
        WebsocketManager::GetInstance()->send(CompletionGenerateClientMessage(
            _components.caretPosition,
            _components.path,
            _components.prefix,
            _components.project,
            _components.recentFiles,
            _components.suffix,
            {}
        ));
    } catch (const runtime_error& e) {
        logger::warn(e.what());
    }
}

void CompletionManager::_threadDebounceRetrieveCompletion() {
    thread([this] {
        while (_isRunning) {
            if (const auto pastTime = chrono::high_resolution_clock::now() - _debounceRetrieveCompletionTime.load();
                pastTime >= chrono::milliseconds(300) && _needRetrieveCompletion.load()) {
                try {
                    const auto caretPosition = InteractionMonitor::GetInstance()->getCaretPosition();
                    const auto getContextLine = [&](const int offset = 0) {
                        return InteractionMonitor::GetInstance()->
                                getLineContent(caretPosition.line + offset);
                    };
                    const auto currentLine = getContextLine();
                    auto prefix = currentLine.substr(0, caretPosition.character);
                    auto suffix = currentLine.substr(caretPosition.character);
                    // if (_isNewLine) {
                    for (auto index = 1; index <= min(caretPosition.line, 30u); ++index) {
                        auto tempLine = getContextLine(-index);
                        prefix.insert(0, tempLine.append("\r\n"));
                        if (regex_match(tempLine, regex(R"(^\/\/[.\W]*)")) ||
                            regex_match(tempLine, regex(R"(^\/\*[.\W]*)"))) {
                            break;
                        }
                    }
                    for (auto index = 1; index <= 5; ++index) {
                        const auto tempLine = getContextLine(index);
                        suffix.append("\r\n").append(tempLine);
                        if (tempLine[0] == '}') {
                            break;
                        }
                    } {
                        unique_lock lock(_componentsMutex);
                        _components.caretPosition = caretPosition;
                        _components.path = InteractionMonitor::GetInstance()->getFileName();
                        _components.prefix = move(prefix);
                        _components.project = InteractionMonitor::GetInstance()->getProjectDirectory();
                        _components.recentFiles = ModificationManager::GetInstance()->getRecentFiles();
                        _components.suffix = move(suffix);
                    }
                    _isNewLine = false;
                    logger::info("Retrieve completion with full prefix");
                    // } else {
                    //     unique_lock lock(_componentsMutex);
                    //     _components.caretPosition = caretPosition;
                    //     _components.path = InteractionMonitor::GetInstance()->getFileName();
                    //     if (const auto lastNewLineIndex = _components.prefix.find_last_of('\r');
                    //         lastNewLineIndex != string::npos) {
                    //         _components.prefix = _components.prefix.substr(0, lastNewLineIndex).append(prefix);
                    //     } else {
                    //         _components.prefix = prefix;
                    //     }
                    //     if (const auto firstNewLineIndex = suffix.find_first_of('\r');
                    //         firstNewLineIndex != string::npos) {
                    //         _components.suffix = _components.suffix.substr(firstNewLineIndex + 1).insert(0, suffix);
                    //     } else {
                    //         _components.suffix = suffix;
                    //     }
                    //     logger::info("Retrieve completion with current line prefix");
                    // }
                    _sendCompletionGenerate();
                    _needRetrieveCompletion.store(false);
                    continue;
                } catch (runtime_error&) {} catch (const exception& e) {
                    logger::warn(format("Exception when retrieving completion: {}", e.what()));
                }
            }
            this_thread::sleep_for(chrono::milliseconds(20));
        }
    }).detach();
}
