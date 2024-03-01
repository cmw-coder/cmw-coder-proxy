#include <chrono>
#include <format>

#include <magic_enum.hpp>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include <components/CompletionManager.h>
#include <components/Configurator.h>
#include <components/MemoryManipulator.h>
#include <components/ModificationManager.h>
#include <components/WebsocketManager.h>
#include <components/WindowManager.h>
#include <types/CaretPosition.h>
#include <utils/iconv.h>
#include <utils/logger.h>

using namespace components;
using namespace helpers;
using namespace magic_enum;
using namespace models;
using namespace std;
using namespace types;
using namespace utils;

namespace {
    const vector<string> keywords = {"class", "if", "for", "struct", "switch", "union", "while"};

    bool checkNeedRetrieveCompletion(const char character) {
        const auto memoryManipulator = MemoryManipulator::GetInstance();
        const auto currentCaretPosition = memoryManipulator->getCaretPosition();
        const auto currentLineContent = memoryManipulator->getLineContent(currentCaretPosition.line);
        if (currentLineContent.empty() || currentCaretPosition.character < currentLineContent.size()) {
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

    vector<SymbolInfo> getDeclaredSymbolInfo(const uint32_t line) {
        const auto memoryManipulator = MemoryManipulator::GetInstance();
        vector<SymbolInfo> declaredSymbols;
        const auto symbolNameOpt = memoryManipulator->getSymbolName(line);
        if (!symbolNameOpt.has_value()) {
            return {};
        }
        const auto childSymbolListHandle = memoryManipulator->getChildSymbolListHandle(symbolNameOpt.value());
        for (uint32_t index = 0; index < childSymbolListHandle->count(); ++index) {
            const auto childSymbolNameOpt = memoryManipulator->getSymbolName(childSymbolListHandle, index);
            if (!childSymbolNameOpt.has_value() ||
                !childSymbolNameOpt.value().depth() ||
                childSymbolNameOpt.value().depth() > 255) {
                continue;
            }

            const auto symbolRecordDeclaredOpt = memoryManipulator->getSymbolRecordDeclared(childSymbolNameOpt.value());
            if (!symbolRecordDeclaredOpt.has_value()) {
                continue;
            }

            const auto symbolDeclaredOpt = symbolRecordDeclaredOpt.value().parse();
            if (!symbolDeclaredOpt.has_value()) {
                continue;
            }
            const auto& [
                file,
                project,
                symbol,
                type,
                namePosition,
                instanceIndex,
                lineEnd,
                lineStart
            ] = symbolDeclaredOpt.value();

            declaredSymbols.emplace_back(symbol, file, type, lineStart, lineEnd - 1);
        }
        memoryManipulator->freeSymbolListHandle(childSymbolListHandle);
        return declaredSymbols;
    }
}

CompletionManager::CompletionManager() {
    _threadDebounceRetrieveCompletion();

    logger::info("CompletionManager is initialized");
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
        const auto memoryManipulator = MemoryManipulator::GetInstance();
        const auto currentPosition = memoryManipulator->getCaretPosition();
        uint32_t insertedlineCount{0}, lastLineLength{0};
        for (const auto lineRange: content.substr(index) | views::split("\r\n"sv)) {
            auto lineContent = string{lineRange.begin(), lineRange.end()};
            if (insertedlineCount == 0) {
                lastLineLength = currentPosition.character + 1 + lineContent.size();
                memoryManipulator->setSelectionContent(lineContent);
            } else {
                lastLineLength = lineContent.size();
                memoryManipulator->setLineContent(currentPosition.line + insertedlineCount, lineContent, true);
            }
            ++insertedlineCount;
        }
        WindowManager::GetInstance()->sendLeftThenRight();
        memoryManipulator->setCaretPosition({lastLineLength, currentPosition.line + insertedlineCount - 1});
        logger::log(format(
            "Accepted completion: {}\n"
            "\tInserted position: ({}, {})",
            content, lastLineLength, currentPosition.line + insertedlineCount - 1
        ));
        WebsocketManager::GetInstance()->send(CompletionAcceptClientMessage(content));
        needBlockMessage = true;
    }
}

void CompletionManager::interactionCompletionCancel(const any& data, bool&) {
    _cancelCompletion();
    logger::log("Cancel completion, Send CompletionCancel");
    try {
        if (any_cast<bool>(data)) {
            _updateNeedRetrieveCompletion();
            WindowManager::GetInstance()->sendF13();
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
    _updateNeedRetrieveCompletion(true, '\n');
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
            _updateNeedRetrieveCompletion(true, character);
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
        _updateNeedRetrieveCompletion(false);
    }
}

void CompletionManager::wsActionCompletionGenerate(const nlohmann::json& data) {
    if (_needDiscardWsAction.load()) {
        logger::log("(WsAction::CompletionGenerate) Ignore due to debounce");
        WebsocketManager::GetInstance()->send(CompletionCancelClientMessage());
        return;
    }
    WindowManager::GetInstance()->unsetMenuText();
    if (const auto result = data["result"].get<string>();
        result == "success") {
        if (const auto& completions = data["completions"];
            completions.is_array() && !completions.empty()) {
            {
                unique_lock lock(_completionListMutex);
                _completionList = completions.get<vector<string>>();
            }
            const auto currentCompletion = _selectCompletion();
            const auto [clientX, clientY] = WindowManager::GetInstance()->getClientPosition();

            auto [height, xPosition, yPosition] = MemoryManipulator::GetInstance()->getCaretDimension();
            while (!height) {
                const auto [
                    newHeight,
                    newXPosition,
                    newYPosition
                ] = MemoryManipulator::GetInstance()->getCaretDimension();
                height = newHeight;
                xPosition = newXPosition;
                yPosition = newYPosition;
            }

            logger::debug(format(
                "Pixels: Client (x: {}, y: {}), Caret (h: {}, x: {}, y: {})",
                clientX, clientY, height, xPosition, yPosition
            ));

            WebsocketManager::GetInstance()->send(CompletionSelectClientMessage(
                currentCompletion,
                0,
                completions.size(),
                clientX + xPosition,
                clientY + yPosition + static_cast<int64_t>(round(0.625 * height))
            ));
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

void CompletionManager::_updateNeedRetrieveCompletion(const bool need, const char character) {
    _prolongRetrieveCompletion();
    _needDiscardWsAction.store(true);
    _needRetrieveCompletion.store(need && (!character || checkNeedRetrieveCompletion(character)));
}

string CompletionManager::_selectCompletion(const uint32_t index) {
    string content; {
        shared_lock lock(_completionListMutex);
        content = _completionList[index];
    } {
        unique_lock lock(_completionCacheMutex);
        _completionCache.reset(iconv::needEncode ? iconv::utf8ToGbk(content) : content);
    }
    return content;
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
            _components.symbols
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
                WindowManager::GetInstance()->setMenuText("Generating...");
                try {
                    WindowManager::GetInstance()->sendF13();
                    const auto memoryManipulator = MemoryManipulator::GetInstance();
                    const auto caretPosition = memoryManipulator->getCaretPosition();
                    auto path = memoryManipulator->getFileName();
                    if (auto project = memoryManipulator->getProjectDirectory();
                        !path.empty() && !project.empty()) {
                        string prefix, suffix; {
                            const auto currentLine = memoryManipulator->getLineContent(caretPosition.line);
                            prefix = currentLine.substr(0, caretPosition.character);
                            suffix = currentLine.substr(caretPosition.character);
                        }
                        for (uint32_t index = 1; index <= min(caretPosition.line, 100u); ++index) {
                            const auto tempLine = memoryManipulator->getLineContent(caretPosition.line - index).append(
                                "\r\n");
                            prefix.insert(0, tempLine);
                        }
                        for (uint32_t index = 1; index <= 30u; ++index) {
                            const auto tempLine = memoryManipulator->getLineContent(caretPosition.line + index);
                            suffix.append("\r\n").append(tempLine);
                        } {
                            unique_lock lock(_componentsMutex);
                            _components.caretPosition = caretPosition;
                            _components.path = move(path);
                            _components.prefix = move(prefix);
                            _components.project = move(project);
                            _components.recentFiles = ModificationManager::GetInstance()->getRecentFiles();
                            _components.suffix = move(suffix);
                            if (Configurator::GetInstance()->version().first == SiVersion::Major::V35) {
                                _components.symbols = getDeclaredSymbolInfo(caretPosition.line);
                            }
                        }
                        _isNewLine = false;
                        logger::info("Retrieve completion with full prefix");
                        // TODO: Improve performance
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
                        _sendCompletionGenerate();
                        _needRetrieveCompletion.store(false);
                    }
                } catch (const exception& e) {
                    logger::warn(format("Exception when retrieving completion: {}", e.what()));
                }
            }
            this_thread::sleep_for(chrono::milliseconds(10));
        }
    }).detach();
}
