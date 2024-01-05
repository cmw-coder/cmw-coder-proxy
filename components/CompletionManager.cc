#include <chrono>
#include <format>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <components/CompletionManager.h>
#include <components/Configurator.h>
#include <components/InteractionMonitor.h>
#include <components/ModificationManager.h>
#include <components/WebsocketManager.h>
#include <components/WindowManager.h>
#include <types/CaretPosition.h>
#include <utils/crypto.h>
#include <utils/logger.h>
#include <utils/system.h>

using namespace components;
using namespace helpers;
using namespace std;
using namespace types;
using namespace utils;

namespace {
    constexpr auto completionGeneratedKey = "CMWCODER_completionGenerated";
}

CompletionManager::CompletionManager() {
    _threadDebounceRetrieveCompletion();
}

void CompletionManager::interactionAcceptCompletion(const any&) {
    // _isContinuousEnter.store(false);
    _isJustAccepted.store(true);
    if (const auto [oldCompletion, cacheOffset] = _completionCache.reset();
        !oldCompletion.content().empty()) {
        {
            shared_lock lock(_componentsMutex);
            system::setEnvironmentVariable(
                completionGeneratedKey,
                (oldCompletion.isSnippet() ? "1" : "0") +
                InteractionMonitor::GetInstance()->getLineContent() +
                oldCompletion.content().substr(cacheOffset)
            );
        }
        WindowManager::GetInstance()->sendAcceptCompletion();
        WebsocketManager::GetInstance()->sendAction(WsAction::CompletionAccept);
        logger::log(format("Accepted completion: {}", oldCompletion.stringify()));
    }
}

void CompletionManager::interactionCaretUpdate(const any& data) {
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

void CompletionManager::interactionDeleteInput(const any&) {
    // _isContinuousEnter.store(false);
    _isJustAccepted.store(false);
    uint32_t chatacter; {
        shared_lock lock(_componentsMutex);
        chatacter = _components.caretPosition.character;
    }
    try {
        if (chatacter != 0) {
            if (const auto previousCacheOpt = _completionCache.previous();
                previousCacheOpt.has_value()) {
                // Has valid cache
                if (const auto [_, completionOpt] = previousCacheOpt.value();
                    completionOpt.has_value()) {
                    WebsocketManager::GetInstance()->sendAction(WsAction::CompletionCache, true);
                    logger::log("Delete backward. Send CompletionCache due to cache hit");
                } else {
                    _cancelCompletion();
                    logger::log("Delete backward. Send CompletionCancel due to cache miss");
                }
            }
        } else if (_completionCache.valid()) {
            _isNewLine = true;
            _cancelCompletion();
            logger::log("Delete backward. Send CompletionCancel due to delete across line");
        }
    } catch (const bad_any_cast& e) {
        logger::log(format("Invalid delayedDelete data: {}", e.what()));
    }
}

void CompletionManager::interactionEnterInput(const any&) {
    _isJustAccepted.store(false);
    _isNewLine = true;
    // TODO: Support 1st level cache
    if (_completionCache.valid()) {
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

void CompletionManager::interactionNavigate(const any& data) {
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

void CompletionManager::interactionNormalInput(const any& data) {
    try {
        const auto character = any_cast<char>(data);
        // _isContinuousEnter.store(false);
        _isJustAccepted.store(false);
        if (const auto nextCacheOpt = _completionCache.next();
            nextCacheOpt.has_value()) {
            // Has valid cache
            const auto [currentChar, completionOpt] = nextCacheOpt.value();
            try {
                if (character == currentChar) {
                    // Cache hit
                    if (completionOpt.has_value()) {
                        // In cache
                        WebsocketManager::GetInstance()->sendAction(WsAction::CompletionCache, false);
                        logger::log("Normal input. Send CompletionCache due to cache hit");
                    } else {
                        // Out of cache
                        _completionCache.reset();
                        WebsocketManager::GetInstance()->sendAction(WsAction::CompletionAccept);
                        logger::log("Normal input. Send CompletionAccept due to cache complete");
                    }
                } else {
                    // Cache miss
                    _cancelCompletion();
                    logger::log("Normal input. Send CompletionCancel due to cache miss");
                    _debounceRetrieveCompletionTime.store(chrono::high_resolution_clock::now());
                    _needRetrieveCompletion.store(true);
                }
            } catch (runtime_error& e) {
                logger::warn(e.what());
            }
        } else {
            // No valid cache
            _debounceRetrieveCompletionTime.store(chrono::high_resolution_clock::now());
            _needRetrieveCompletion.store(true);
        }
    } catch (const bad_any_cast& e) {
        logger::warn(format("Invalid interactionNormalInput data: {}", e.what()));
    }
}

void CompletionManager::interactionSave(const any&) {
    if (_completionCache.valid()) {
        _cancelCompletion();
        logger::log("Save. Send CompletionCancel");
    }
}

void CompletionManager::instantUndo(const any&) {
    // _isContinuousEnter.store(false);
    _isNewLine = true;
    if (_isJustAccepted.load()) {
        _isJustAccepted.store(false);
    } else if (_completionCache.valid()) {
        _cancelCompletion();
        logger::log("Undo. Send CompletionCancel");
    } else {
        // Invalidate current retrieval
        _debounceRetrieveCompletionTime.store(chrono::high_resolution_clock::now());
        _needRetrieveCompletion.store(false);
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
            const auto completionsDecodes = decode(completions[0].get<string>(), crypto::Encoding::Base64);
            _completionCache.reset(
                completionsDecodes[0] == '1',
                completionsDecodes.substr(1)
            );
        } else {
            logger::log("(WsAction::CompletionGenerate) No completion");
        }
    } else {
        logger::log(format(
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
    _completionCache.reset();
}

void CompletionManager::_sendCompletionGenerate() {
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
}

void CompletionManager::_threadDebounceRetrieveCompletion() {
    thread([this] {
        while (_isRunning) {
            if (_needRetrieveCompletion.load()) {
                if (const auto pastTime = chrono::high_resolution_clock::now() - _debounceRetrieveCompletionTime.load();
                    pastTime >= chrono::milliseconds(500)) {
                    const auto currentCaretPosition = InteractionMonitor::GetInstance()->getCaretPosition();
                    const auto getContextLine = [&](const int offset = 0) {
                        return InteractionMonitor::GetInstance()->
                                getLineContent(currentCaretPosition.line + offset);
                    };
                    if (_isNewLine) {
                        auto prefix = getContextLine().substr(0, currentCaretPosition.character);
                        auto suffix = getContextLine().substr(currentCaretPosition.character);
                        for (auto index = 1; index <= 30; ++index) {
                            prefix.insert(0, getContextLine(-index)).insert(0, "\r\n");
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
                        auto prefix = getContextLine().substr(0, currentCaretPosition.character);
                        auto suffix = getContextLine().substr(currentCaretPosition.character);
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
                }
            } else {
                this_thread::sleep_for(chrono::milliseconds(10));
            }
        }
    }).detach();
}
