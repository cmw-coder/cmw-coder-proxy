#include <chrono>
#include <format>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <components/CompletionManager.h>
#include <components/Configurator.h>
#include <components/WindowManager.h>
#include <types/CursorPosition.h>
#include <utils/crypto.h>
#include <utils/logger.h>
#include <utils/system.h>

using namespace components;
using namespace std;
using namespace types;
using namespace utils;

namespace {
    constexpr auto cancelTypeKey = "CMWCODER_cancelType";
    constexpr auto completionGeneratedKey = "CMWCODER_completionGenerated";

    constexpr auto insertCompletion = [](const std::string&completionString) {
        system::setEnvironmentVariable(completionGeneratedKey, completionString);
        WindowManager::GetInstance()->sendInsertCompletion();
    };
}

CompletionManager::CompletionManager(): _httpHelper("http://localhost:3000", chrono::seconds(10)) {
}

void CompletionManager::interactionAccept(const std::any&) {
    _isContinuousEnter.store(false);
    _isJustAccepted.store(true);
    if (auto oldCompletion = _completionCache.reset(); !oldCompletion.content().empty()) {
        WindowManager::GetInstance()->sendAcceptCompletion();
        logger::log(format("Accepted completion: {}", oldCompletion.stringify()));
        thread(&CompletionManager::_reactToCompletion, this, move(oldCompletion), true).detach();
    }
    else {
        _retrieveEditorInfo();
    }
}

void CompletionManager::interactionCancel(const std::any&data) {
    try {
        const auto [isCrossLine,isNeedReset] = any_cast<tuple<bool, bool>>(data);
        _cancelCompletion(isCrossLine, isNeedReset);
    }
    catch (const bad_any_cast&e) {
        logger::log(format("Invalid interactionCancel data: {}", e.what()));
    }
}

void CompletionManager::interactionDelete(const std::any&data) {
    _isContinuousEnter.store(false);
    try {
        if (const auto [newCursorPosition, oldCursorPosition] = any_cast<tuple<CaretPosition, CaretPosition>>(data);
            newCursorPosition.line == oldCursorPosition.line) {
            if (const auto previousCacheOpt = _completionCache.previous(); previousCacheOpt.has_value()) {
                // Has valid cache
                const auto [_, completionOpt] = previousCacheOpt.value();
                try {
                    if (completionOpt.has_value()) {
                        // In cache
                        _cancelCompletion(false, false);
                        logger::log("Cancel completion due to previous cached");
                        insertCompletion(completionOpt.value().stringify());
                        logger::log("Insert previous cached completion");
                    }
                    else {
                        // Out of cache
                        _cancelCompletion();
                        logger::log("Canceled by delete backward.");
                    }
                }
                catch (runtime_error&e) {
                    logger::log(e.what());
                }
            }
        }
        else {
            if (_completionCache.valid()) {
                _cancelCompletion(true);
                _isContinuousEnter.store(false);
                logger::log("Canceled by backspace");
            }
        }
    }
    catch (const bad_any_cast&e) {
        logger::log(format("Invalid interactionDelete data: {}", e.what()));
    }
}

void CompletionManager::interactionEnter(const std::any&) {
    if (_completionCache.valid()) {
        // TODO: support 1st level cache
        _cancelCompletion(true);
        logger::log("Canceled by enter");
    }

    if (_isContinuousEnter) {
        shared_lock lock(_componentsMutex);
        _retrieveCompletion(_components.prefix);
        logger::log("Detect Continuous enter, retrieve completion directly");
    }
    else {
        _isContinuousEnter.store(true);
        _retrieveEditorInfo();
        logger::log("Detect first enter, retrieve editor info first");
    }
}

void CompletionManager::interactionNavigate(const std::any&) {
    _isContinuousEnter.store(false);
    if (_completionCache.valid()) {
        _cancelCompletion();
        logger::log("Canceled by navigate.");
    }
}

void CompletionManager::interactionNormal(const std::any&data) {
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
                        _cancelCompletion(false, false);
                        logger::log("Cancel completion due to next cached");
                        insertCompletion(completionOpt.value().stringify());
                        logger::log("Insert next cached completion");
                    }
                    else {
                        // Out of cache
                        interactionAccept();
                    }
                }
                else {
                    // Cache miss
                    _cancelCompletion();
                    logger::log(format("Canceled due to cache miss"));
                    _retrieveEditorInfo();
                }
            }
            catch (runtime_error&e) {
                logger::log(e.what());
            }
        }
        else {
            // No valid cache
            _retrieveEditorInfo();
        }
    }
    catch (const bad_any_cast&e) {
        logger::log(format("Invalid interactionNormal data: {}", e.what()));
    }
}

void CompletionManager::interactionSave(const std::any&) {
    if (_completionCache.valid()) {
        _cancelCompletion();
        logger::log("Canceled by save.");
        WindowManager::GetInstance()->sendSave();
    }
    else {
        // Interrupted current retrieve
        _currentRetrieveTime.store(chrono::high_resolution_clock::now());
    }
}

void CompletionManager::interactionUndo(const std::any&) {
    _isContinuousEnter.store(false);
    const auto windowManager = WindowManager::GetInstance();
    if (_isJustAccepted.load()) {
        _isJustAccepted.store(false);
        windowManager->sendUndo();
        windowManager->sendUndo();
    }
    else if (_completionCache.valid()) {
        _completionCache.reset();
        logger::log("Canceled by undo");
        windowManager->sendUndo();
    }
    else {
        // Interrupted current retrieve
        _currentRetrieveTime.store(chrono::high_resolution_clock::now());
    }
}

void CompletionManager::retrieveWithCurrentPrefix(const std::string&currentPrefix) {
    shared_lock lock(_componentsMutex);
    if (const auto lastNewLineIndex = _components.prefix.find_last_of('\n'); lastNewLineIndex != string::npos) {
        _retrieveCompletion(_components.prefix.substr(0, lastNewLineIndex + 1) + currentPrefix);
    }
    else {
        _retrieveCompletion(currentPrefix);
    }
}

void CompletionManager::retrieveWithFullInfo(Components&&components) {
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

void CompletionManager::setProjectId(const std::string&projectId) {
    bool needSet; {
        shared_lock lock(_editorInfoMutex);
        needSet = _editorInfo.projectId != projectId;
    }
    if (needSet) {
        unique_lock lock(_editorInfoMutex);
        _editorInfo.projectId = projectId;
    }
}

void CompletionManager::setVersion(const std::string&version) {
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

void CompletionManager::_cancelCompletion(const bool isCrossLine, const bool isNeedReset) {
    try {
        system::setEnvironmentVariable(cancelTypeKey, to_string(isCrossLine));
        WindowManager::GetInstance()->sendCancelCompletion();
        if (isNeedReset) {
            _completionCache.reset();
        }
    }
    catch (runtime_error&e) {
        logger::log(e.what());
    }
}

void CompletionManager::_reactToCompletion(Completion&&completion, const bool isAccept) {
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
        if (const auto [status, responseBody] = _httpHelper.post(path, move(requestBody)); status == 200) {
            logger::log(format("({}) Result: {}", path, responseBody["result"].get<string>()));
        }
        else {
            logger::log(
                format("({}) HTTP Code: {}, body: {}", path, status, responseBody.dump())
            );
        }
    }
    catch (helpers::HttpHelper::HttpError&e) {
        logger::error(format("(/completion/accept) Http error: {}", e.what()));
    }
    catch (exception&e) {
        logger::error(format("(/completion/accept) Exception: {}", e.what()));
    }
}

void CompletionManager::_retrieveCompletion(const std::string&prefix) {
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
            if (const auto [status, responseBody] = _httpHelper.post("/completion/generate", move(requestBody));
                status == 200) {
                const auto result = responseBody["result"].get<string>();
                if (const auto&contents = responseBody["contents"];
                    result == "success" && contents.is_array() && !contents.empty()) {
                    completionGenerated.emplace(decode(contents[0].get<string>(), crypto::Encoding::Base64));
                    logger::log(format("(/completion/generate) Completion: {}", completionGenerated.value_or("null")));
                }
                else {
                    logger::log(format("(/completion/generate) Invalid completion: {}", responseBody.dump()));
                }
            }
            else {
                logger::log(format("(/completion/generate) HTTP Code: {}, body: {}", status, responseBody.dump()));
            }
        }
        catch (helpers::HttpHelper::HttpError&e) {
            logger::error(format("(/completion/accept) Http error: {}", e.what()));
        }
        catch (exception&e) {
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
                _cancelCompletion(false, false);
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
