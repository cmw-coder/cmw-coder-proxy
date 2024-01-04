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

optional<string> CompletionManager::acceptCompletion(const int line) {
    _isContinuousEnter.store(false);
    _isJustAccepted.store(true);
    if (const auto [oldCompletion, cacheOffset] = _completionCache.reset();
        !oldCompletion.content().empty()) {
        system::setEnvironmentVariable(
            completionGeneratedKey,
            (oldCompletion.isSnippet() ? "1" : "0") +
            InteractionMonitor::GetInstance()->getLineContent(line) +
            oldCompletion.content().substr(cacheOffset)
        );
        WindowManager::GetInstance()->sendAcceptCompletion();
        logger::log(format("Accepted completion: {}", oldCompletion.stringify()));
        WebsocketManager::GetInstance()->sendAction(WsAction::CompletionAccept);
        return oldCompletion.content();
    }
    return nullopt;
}

void CompletionManager::cancelCompletion() {
    _isContinuousEnter.store(false);
    _cancelCompletion();
}

void CompletionManager::deleteInput(const CaretPosition& position) {
    _isContinuousEnter.store(false);
    try {
        if (position.character != 0) {
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
            _cancelCompletion();
            logger::log("Delete backward. Send CompletionCancel due to delete across line");
        }
    } catch (const bad_any_cast& e) {
        logger::log(format("Invalid delayedDelete data: {}", e.what()));
    }
}

bool CompletionManager::normalInput(const char character) {
    bool needAccept = false;
    _isContinuousEnter.store(false);
    _isJustAccepted.store(false);
    try {
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
                        needAccept = true;
                    }
                } else {
                    // Cache miss
                    _cancelCompletion();
                    logger::log("Normal input. Send CompletionCancel due to cache miss");
                    if (character == '\n') {
                        _isContinuousEnter.store(true);
                    }
                    _retrieveEditorInfo();
                }
            } catch (runtime_error& e) {
                logger::log(e.what());
            }
        } else {
            // No valid cache
            if (character == '\n') {
                // TODO: Implement logic for continuous enter
                if (_isContinuousEnter) {
                    shared_lock lock(_componentsMutex);
                    _retrieveCompletion(_components.prefix);
                    logger::log("Detect Continuous enter, retrieve completion directly");
                } else {
                    _isContinuousEnter.store(true);
                    _retrieveEditorInfo();
                    logger::log("Detect first enter, retrieve editor info first");
                }
            } else {
                _retrieveEditorInfo();
            }
        }
    } catch (const bad_any_cast& e) {
        logger::log(format("Invalid normalInput data: {}", e.what()));
    }
    return needAccept;
}

void CompletionManager::instantUndo(const any&) {
    _isContinuousEnter.store(false);
    if (_isJustAccepted.load()) {
        _isJustAccepted.store(false);
    } else if (_completionCache.valid()) {
        _cancelCompletion();
        logger::log("Undo. Send CompletionCancel");
    } else {
        // Interrupt current retrieve
        _currentRetrieveTime.store(chrono::high_resolution_clock::now());
    }
}

void CompletionManager::wsActionCompletionGenerate(const nlohmann::json& data) {
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

void CompletionManager::retrieveWithCurrentPrefix(const string& currentPrefix) {
    shared_lock lock(_componentsMutex);
    if (const auto lastNewLineIndex = _components.prefix.find_last_of('\n');
        lastNewLineIndex != string::npos) {
        _retrieveCompletion(_components.prefix.substr(0, lastNewLineIndex + 1) + currentPrefix);
    } else {
        _retrieveCompletion(currentPrefix);
    }
}

void CompletionManager::retrieveWithFullInfo(Components&& components) {
    unique_lock lock(_componentsMutex);
    _components = move(components);
    _retrieveCompletion(_components.prefix);
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

void CompletionManager::_cancelCompletion(const bool isNeedReset) {
    WebsocketManager::GetInstance()->sendAction(WsAction::CompletionCancel);
    if (isNeedReset) {
        _completionCache.reset();
    }
}

void CompletionManager::_retrieveCompletion(const string& prefix) {
    _currentRetrieveTime.store(chrono::high_resolution_clock::now());
    // TODO: replace this with better solution
    const auto currentPosition = ModificationManager::GetInstance()->getCurrentPosition();
    const auto [xPixel, yPixel] = InteractionMonitor::GetInstance()->getCaretPixels(
        static_cast<int>(currentPosition.line)
    );
    WebsocketManager::GetInstance()->sendAction(
        WsAction::CompletionGenerate,
        {
            {
                "caret", {
                    {"character", currentPosition.character},
                    {"line", currentPosition.line},
                    {"xPixel", xPixel},
                    {"yPixel", yPixel},
                }
            },
            {"path", encode(_components.path, crypto::Encoding::Base64)},
            {"prefix", encode(prefix, crypto::Encoding::Base64)},
            {"projectId", _editorInfo.projectId},
            {"suffix", encode(_components.suffix, crypto::Encoding::Base64)},
            {"symbolString", encode(_components.symbolString, crypto::Encoding::Base64)},
            {"tabString", encode(_components.tabString, crypto::Encoding::Base64)},
        }
    );
}

void CompletionManager::_retrieveEditorInfo() const {
    if (_isAutoCompletion.load()) {
        WindowManager::GetInstance()->requestRetrieveInfo();
    }
}
