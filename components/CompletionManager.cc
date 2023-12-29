#include <chrono>
#include <format>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <components/CompletionManager.h>
#include <components/Configurator.h>
#include <components/WebsocketManager.h>
#include <components/WindowManager.h>
#include <types/CaretPosition.h>
#include <utils/crypto.h>
#include <utils/logger.h>
#include <utils/system.h>

#include "ModificationManager.h"

using namespace components;
using namespace helpers;
using namespace std;
using namespace types;
using namespace utils;

namespace {
    constexpr auto cancelTypeKey = "CMWCODER_cancelType";
    constexpr auto completionGeneratedKey = "CMWCODER_completionGenerated";

    constexpr auto insertCompletion = [](const string& completionString) {
        system::setEnvironmentVariable(completionGeneratedKey, completionString);
        WindowManager::GetInstance()->sendInsertCompletion();
    };

    void cacheCompletion(const char character) {
        if (character < 0) {
            WebsocketManager::GetInstance()->sendAction(
                WsAction::CompletionCache,
                {
                    {"isDelete", true},
                }
            );
        } else {
            WebsocketManager::GetInstance()->sendAction(
                WsAction::CompletionCache,
                {
                    {"character", string(1, character)},
                    {"isDelete", false},
                }
            );
        }
    }
}

void CompletionManager::interactionCaretUpdate(const std::any& data) {
    try {
        const auto [newCursorPosition, oldCursorPosition] = any_cast<tuple<CaretPosition, CaretPosition>>(data);
        _lastPosition.store(newCursorPosition);
    } catch (const bad_any_cast& e) {
        logger::log(format("Invalid instantCaret data: {}", e.what()));
    }
    catch (out_of_range&) {}
}

void CompletionManager::delayedDelete(const any&) {
    _isContinuousEnter.store(false);
    const auto currentPosition = _lastPosition.load();
    try {
        if (currentPosition.character != 0) {
            if (const auto previousCacheOpt = _completionCache.previous();
                previousCacheOpt.has_value()) {
                // Has valid cache
                if (const auto [_, completionOpt] = previousCacheOpt.value();
                    completionOpt.has_value()) {
                    cacheCompletion(-1);
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

void CompletionManager::delayedEnter(CaretPosition, CaretPosition, const any&) {
    if (_completionCache.valid()) {
        // TODO: support 1st level cache
        _cancelCompletion();
        logger::log("Input new line. Send CompletionCancel");
    }

    if (_isContinuousEnter) {
        shared_lock lock(_componentsMutex);
        _retrieveCompletion(_components.prefix);
        logger::log("Detect Continuous enter, retrieve completion directly");
    } else {
        _isContinuousEnter.store(true);
        _retrieveEditorInfo();
        logger::log("Detect first enter, retrieve editor info first");
    }
}

void CompletionManager::delayedNavigate(CaretPosition, CaretPosition, const any&) {
    instantNavigate();
}

void CompletionManager::instantAccept(const any&) {
    _isContinuousEnter.store(false);
    _isJustAccepted.store(true);
    if (auto oldCompletion = _completionCache.reset(); !oldCompletion.content().empty()) {
        WindowManager::GetInstance()->sendAcceptCompletion();
        logger::log(format("Accepted completion: {}", oldCompletion.stringify()));
        thread(&CompletionManager::_reactToCompletion, this, move(oldCompletion), true).detach();
    } else {
        // _retrieveEditorInfo();
        ModificationManager::GetInstance()->instantNavigate(Key::Tab);
    }
}

void CompletionManager::instantCancel(const any& data) {
    try {
        _cancelCompletion(any_cast<bool>(data));
    } catch (const bad_any_cast& e) {
        logger::log(format("Invalid interactionCancel data: {}", e.what()));
    }
}

void CompletionManager::instantNavigate(const std::any&) {
    _isContinuousEnter.store(false);
    if (_completionCache.valid()) {
        _cancelCompletion();
        logger::log("Canceled by navigate.");
    }
}

void CompletionManager::instantNormal(const any& data) {
    _isContinuousEnter.store(false);
    _isJustAccepted.store(false);
    try {
        const auto keycode = any_cast<Keycode>(data);
        if (const auto nextCacheOpt = _completionCache.next();
            nextCacheOpt.has_value()) {
            // Has valid cache
            const auto [currentChar, completionOpt] = nextCacheOpt.value();
            try {
                // TODO: Check if this would cause issues
                if (keycode == currentChar) {
                    // Cache hit
                    if (completionOpt.has_value()) {
                        // In cache
                        _cancelCompletion(false);
                        logger::log("Cancel completion due to next cached");
                        insertCompletion(completionOpt.value().stringify());
                        logger::log("Insert next cached completion");
                    } else {
                        // Out of cache
                        instantAccept();
                    }
                } else {
                    // Cache miss
                    _cancelCompletion();
                    logger::log(format("Canceled due to cache miss"));
                    _retrieveEditorInfo();
                }
            } catch (runtime_error& e) {
                logger::log(e.what());
            }
        } else {
            // No valid cache
            _retrieveEditorInfo();
        }
    } catch (const bad_any_cast& e) {
        logger::log(format("Invalid interactionNormal data: {}", e.what()));
    }
}

void CompletionManager::instantSave(const any&) {
    if (_completionCache.valid()) {
        _cancelCompletion();
        logger::log("Canceled by save.");
        WindowManager::GetInstance()->sendSave();
    } else {
        // Interrupted current retrieve
        _currentRetrieveTime.store(chrono::high_resolution_clock::now());
    }
}

void CompletionManager::instantUndo(const any&) {
    _isContinuousEnter.store(false);
    const auto windowManager = WindowManager::GetInstance();
    if (_isJustAccepted.load()) {
        _isJustAccepted.store(false);
        windowManager->sendUndo();
        windowManager->sendUndo();
    } else if (_completionCache.valid()) {
        _completionCache.reset();
        logger::log("Canceled by undo");
        windowManager->sendUndo();
    } else {
        // Interrupted current retrieve
        _currentRetrieveTime.store(chrono::high_resolution_clock::now());
    }
}

void CompletionManager::retrieveWithCurrentPrefix(const string& currentPrefix) {
    shared_lock lock(_componentsMutex);
    if (const auto lastNewLineIndex = _components.prefix.find_last_of('\n'); lastNewLineIndex != string::npos) {
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

void CompletionManager::_reactToCompletion(Completion&& completion, const bool isAccept) {
    const auto path = isAccept ? "/completion/accept" : "/completion/insert";
    nlohmann::json requestBody; {
        shared_lock lock(_editorInfoMutex);
        requestBody = {
            {"completion", encode(completion.stringify(), crypto::Encoding::Base64)},
            {"projectId", _editorInfo.projectId},
            {"version", _editorInfo.version},
        };
    }
    try {
        if (const auto [status, responseBody] = HttpHelper(
                "http://localhost:3000", chrono::seconds(10)
            ).post(path, move(requestBody));
            status == 200) {
            logger::log(format("({}) Result: {}", path, responseBody["result"].get<string>()));
        } else {
            logger::log(
                format("({}) HTTP Code: {}, body: {}", path, status, responseBody.dump())
            );
        }
    } catch (HttpHelper::HttpError& e) {
        logger::error(format("(/completion/accept) Http error: {}", e.what()));
    }
    catch (exception& e) {
        logger::error(format("(/completion/accept) Exception: {}", e.what()));
    }
}

void CompletionManager::_retrieveCompletion(const string& prefix) {
    _currentRetrieveTime.store(chrono::high_resolution_clock::now());
    thread([this, prefix, originalRetrieveTime = _currentRetrieveTime.load()] {
        optional<string> completionGenerated;
        nlohmann::json requestBody; {
            shared_lock componentsLock(_componentsMutex);
            shared_lock editorInfoLock(_editorInfoMutex);
            requestBody = {
                {"cursorString", _components.cursorString},
                {"path", encode(_components.path, crypto::Encoding::Base64)},
                {"prefix", encode(prefix, crypto::Encoding::Base64)},
                {"projectId", _editorInfo.projectId},
                {"suffix", encode(_components.suffix, crypto::Encoding::Base64)},
                {"symbolString", encode(_components.symbolString, crypto::Encoding::Base64)},
                {"tabString", encode(_components.tabString, crypto::Encoding::Base64)},
                {"version", _editorInfo.version},
            };
        }
        try {
            if (const auto [status, responseBody] = HttpHelper(
                    "http://localhost:3000", chrono::seconds(10)
                ).post("/completion/generate", move(requestBody));
                status == 200) {
                const auto result = responseBody["result"].get<string>();
                if (const auto& contents = responseBody["contents"];
                    result == "success" && contents.is_array() && !contents.empty()) {
                    completionGenerated.emplace(decode(contents[0].get<string>(), crypto::Encoding::Base64));
                    logger::log(format("(/completion/generate) Completion: {}", completionGenerated.value_or("null")));
                } else {
                    logger::log(format("(/completion/generate) Invalid completion: {}", responseBody.dump()));
                }
            } else {
                logger::log(format("(/completion/generate) HTTP Code: {}, body: {}", status, responseBody.dump()));
            }
        } catch (HttpHelper::HttpError& e) {
            logger::error(format("(/completion/accept) Http error: {}", e.what()));
        }
        catch (exception& e) {
            logger::log(format("(/completion/generate) Exception: {}", e.what()));
        }
        catch (...) {
            logger::log("(/completion/generate) Unknown exception.");
        }

        if (
            completionGenerated.has_value() &&
            originalRetrieveTime == _currentRetrieveTime.load()
        ) {
            auto oldCompletion = _completionCache.reset(
                completionGenerated.value()[0] == '1',
                completionGenerated.value().substr(1)
            );
            if (!oldCompletion.content().empty()) {
                _cancelCompletion(false);
                logger::log(format("Cancel old cached completion: {}", oldCompletion.stringify()));
            }
            insertCompletion(completionGenerated.value());
            logger::log("Inserted completion");
            thread(&CompletionManager::_reactToCompletion, this, move(oldCompletion), false).detach();
        }
    }).detach();
}

void CompletionManager::_retrieveEditorInfo() const {
    if (_isAutoCompletion.load()) {
        WindowManager::GetInstance()->requestRetrieveInfo();
    }
}
