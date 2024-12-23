#include <chrono>
#include <format>
#include <regex>

#include <magic_enum/magic_enum.hpp>
#include <nlohmann/json.hpp>

#include <components/CompletionManager.h>
#include <components/ConfigManager.h>
#include <components/InteractionMonitor.h>
#include <components/MemoryManipulator.h>
#include <components/SymbolManager.h>
#include <components/WebsocketManager.h>
#include <components/WindowManager.h>
#include <types/CaretPosition.h>
#include <types/CompletionComponents.h>
#include <utils/common.h>
#include <utils/iconv.h>
#include <utils/logger.h>
#include <utils/system.h>

using namespace components;
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
        const auto currentFileHandle = memoryManipulator->getHandle(MemoryAddress::HandleType::File);
        // TODO: Make this method safer
        const auto currentLineContent = memoryManipulator->getLineContent(currentFileHandle, currentCaretPosition.line);
        if (currentLineContent.empty() || currentCaretPosition.character < currentLineContent.size()) {
            return false;
        }
        switch (character) {
            case ';': {
                if (ranges::none_of(keywords, [&currentLineContent](const string& keyword) {
                    return regex_search(currentLineContent, regex(format(R"~(\b{}\b)~", keyword)));
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

    bool fileTimeCompare(
        const CompletionManager::FileTime& a,
        const CompletionManager::FileTime& b
    ) {
        return a.second > b.second;
    }
}

CompletionManager::CompletionManager() {
    ranges::make_heap(_recentFiles, fileTimeCompare);
    _threadCheckAcceptedCompletions();
    _threadCheckCurrentFilePath();
    _threadDebounceRetrieveCompletion();
    logger::info("CompletionManager is initialized");
}

CompletionManager::~CompletionManager() {
    _isRunning = false;
}

void CompletionManager::interactionCompletionAccept(const any&, bool& needBlockMessage) {
    CompletionComponents::GenerateType generateType;
    Selection selection{};
    string actionId, content;
    int64_t cacheIndex; {
        unique_lock lock(_completionCacheMutex);
        tie(content, cacheIndex) = _completionCache.reset();
    } {
        shared_lock lock(_completionsMutex);
        const auto& completions = _completionsOpt.value();
        generateType = completions.generateType;
        selection = completions.selection;
        actionId = completions.actionId;
    }
    if (!content.empty()) {
        {
            // TODO: Adapt to CompletionComponents::GenerateType::PasteReplace
            unique_lock lock(_editedCompletionMapMutex);
            if (_editedCompletionMap.contains(actionId)) {
                _editedCompletionMap.at(actionId).react(true);
            }
        }
        switch (generateType) {
            case CompletionComponents::GenerateType::Common: {
                common::insertContent(content.substr(cacheIndex));
                break;
            }
            case CompletionComponents::GenerateType::PasteReplace: {
                common::replaceContent(selection, content);
                break;
            }
        }
        shared_lock lock(_completionsMutex);
        WebsocketManager::GetInstance()->send(CompletionAcceptClientMessage(
            _completionsOpt.value().actionId,
            get<1>(_completionsOpt.value().current())
        ));
        needBlockMessage = true;
    }
}

void CompletionManager::interactionCompletionCancel(const any& data, bool& needBlockMessage) {
    const auto hasCompletion = _cancelCompletion();
    logger::log("Cancel completion, Send CompletionCancel");
    try {
        if (any_cast<bool>(data)) {
            _updateNeedRetrieveCompletion();
            WindowManager::GetInstance()->sendF13();
        }
    } catch (const bad_any_cast& e) {
        logger::warn(format("Invalid interactionCompletionCancel data: {}", e.what()));
    }
    needBlockMessage = hasCompletion;
}

void CompletionManager::interactionDeleteInput(const any&, bool&) {
    const auto [character, line, _] = MemoryManipulator::GetInstance()->getCaretPosition();
    try {
        if (character != 0) {
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
        } else {
            if (_hasValidCache()) {
                _cancelCompletion();
                logger::log("Delete backward. Send CompletionCancel due to delete across line");
            }
            if (const auto currentWindowHandleOpt = WindowManager::GetInstance()->getCurrentWindowHandle();
                currentWindowHandleOpt.has_value()) {
                unique_lock lock(_editedCompletionMapMutex);
                for (auto& editedCompletion: _editedCompletionMap | views::values) {
                    if (editedCompletion.windowHandle == currentWindowHandleOpt.value()) {
                        editedCompletion.removeLine(line);
                    }
                }
            }
        }
    } catch (const bad_any_cast& e) {
        logger::log(format("Invalid delayedDelete data: {}", e.what()));
    }
}

void CompletionManager::interactionEnterInput(const any&, bool&) {
    // TODO: Support 1st level cache
    if (_hasValidCache()) {
        _cancelCompletion();
        logger::log("Enter Input. Send CompletionCancel");
    }
    _updateNeedRetrieveCompletion(true, '\n');
    if (const auto currentWindowHandleOpt = WindowManager::GetInstance()->getCurrentWindowHandle();
        currentWindowHandleOpt.has_value()) {
        const auto line = MemoryManipulator::GetInstance()->getCaretPosition().line;
        unique_lock lock(_editedCompletionMapMutex);
        for (auto& editedCompletion: _editedCompletionMap | views::values) {
            if (editedCompletion.windowHandle == currentWindowHandleOpt.value()) {
                editedCompletion.addLine(line);
            }
        }
    }
}

void CompletionManager::interactionNavigateWithKey(const any&, bool&) {
    try {
        if (_hasValidCache()) {
            _cancelCompletion();
            logger::log("Navigate with key. Send CompletionCancel");
        }
    } catch (const bad_any_cast& e) {
        logger::log(format("Invalid interactionNavigateWithKey data: {}", e.what()));
    }
}

void CompletionManager::interactionNavigateWithMouse(const any& data, bool&) {
    try {
        const auto [newCursorPosition, _] = any_cast<tuple<CaretPosition, CaretPosition>>(data); {
            shared_lock componentsLock(_lastCaretPositionMutex);
            if (_lastCaretPosition != newCursorPosition) {
                if (_hasValidCache()) {
                    _cancelCompletion();
                    logger::log("Navigate with mouse. Send CompletionCancel");
                }
            }
        }
        unique_lock lock(_lastCaretPositionMutex);
        _lastCaretPosition = newCursorPosition;
    } catch (const bad_any_cast& e) {
        logger::log(format("Invalid interactionNavigateWithMouse data: {}", e.what()));
    }
}

void CompletionManager::interactionNormalInput(const any& data, bool&) {
    try {
        bool needRetrieveCompletion = false;
        const auto character = any_cast<char>(data);
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
                } else {
                    // Out of cache
                    _completionCache.reset(); {
                        shared_lock lock(_completionsMutex);
                        WebsocketManager::GetInstance()->send(CompletionAcceptClientMessage(
                            _completionsOpt.value().actionId,
                            get<1>(_completionsOpt.value().current())
                        ));
                    }
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

    if (const auto clipboardTextOpt = system::getClipboardText();
        clipboardTextOpt.has_value()) {
        const auto& clipboardText = iconv::autoDecode(clipboardTextOpt.value());
        const auto memoryManipulator = MemoryManipulator::GetInstance();
        const auto caretPosition = memoryManipulator->getCaretPosition();
        if (const auto currentWindowHandleOpt = WindowManager::GetInstance()->getCurrentWindowHandle();
            currentWindowHandleOpt.has_value()) {
            const auto addedLineCount = caretPosition.character == 0
                                            ? common::countLines(clipboardText)
                                            : common::countLines(clipboardText) - 1;
            unique_lock lock(_editedCompletionMapMutex);
            for (auto& editedCompletion: _editedCompletionMap | views::values) {
                if (editedCompletion.windowHandle == currentWindowHandleOpt.value()) {
                    editedCompletion.addLine(caretPosition.line, addedLineCount);
                }
            }
        }

        try {
            if (const auto completionComponentsOpt = _retrieveContext(
                caretPosition,
                CompletionComponents::GenerateType::PasteReplace,
                clipboardText
            ); completionComponentsOpt.has_value()) {
                const auto& completionComponents = completionComponentsOpt.value();
                WebsocketManager::GetInstance()->send(EditorPasteClientMessage(
                    caretPosition,
                    clipboardText,
                    completionComponents.getPrefix(),
                    completionComponents.getSuffix(),
                    completionComponents.getRecentFiles()
                ));

                if (const auto lineCount = common::countLines(clipboardText);
                    lineCount == 1) {
                    const auto lastNonSpaceIndex = clipboardText.find_last_not_of(" \t");
                    const auto lastNonSpaceChar = lastNonSpaceIndex == string::npos
                                                      ? clipboardText.back()
                                                      : clipboardText[lastNonSpaceIndex];
                    if (lastNonSpaceChar != ';') {
                        _prolongRetrieveCompletion();
                        _needDiscardWsAction.store(true);
                        _needRetrieveCompletion.store(true);
                    } else {
                        _sendGenerateMessage(completionComponents);
                    }
                } else if (lineCount < _configPasteMaxLineCount.load()) {
                    _sendGenerateMessage(completionComponents);
                }
            }
        } catch (const exception& e) {
            logger::warn(format("(interactionPaste) Exception: {}", e.what()));
        }
    }
}

void CompletionManager::interactionSave(const any&, bool&) {
    if (_hasValidCache()) {
        _cancelCompletion();
        logger::log("Save. Send CompletionCancel");
    }
}

void CompletionManager::interactionSelectionReplace(const any& data, bool&) {
    try {
        if (const auto [startLine, count] = any_cast<pair<uint32_t, int32_t>>(data);
            count > 0) {
            if (const auto currentWindowHandleOpt = WindowManager::GetInstance()->getCurrentWindowHandle();
                currentWindowHandleOpt.has_value()) {
                unique_lock lock(_editedCompletionMapMutex);
                for (auto& editedCompletion: _editedCompletionMap | views::values) {
                    if (editedCompletion.windowHandle == currentWindowHandleOpt.value()) {
                        editedCompletion.addLine(startLine, count);
                    }
                }
            }
        } else if (count < 0) {
            if (const auto currentWindowHandleOpt = WindowManager::GetInstance()->getCurrentWindowHandle();
                currentWindowHandleOpt.has_value()) {
                unique_lock lock(_editedCompletionMapMutex);
                for (auto& editedCompletion: _editedCompletionMap | views::values) {
                    if (editedCompletion.windowHandle == currentWindowHandleOpt.value()) {
                        editedCompletion.removeLine(startLine, -count);
                    }
                }
            }
        }
    } catch (const bad_any_cast& e) {
        logger::log(format("Invalid interactionSelectionReplace data: {}", e.what()));
    }
}

void CompletionManager::interactionUndo(const any&, bool&) {
    if (_hasValidCache()) {
        _cancelCompletion();
        logger::log("Undo. Send CompletionCancel");
    } else {
        // Invalidate current retrieval
        _updateNeedRetrieveCompletion(false);
    }
}

void CompletionManager::updateCompletionConfig(const CompletionConfig& completionConfig) {
    if (const auto completionOnPasteOpt = completionConfig.completionOnPaste;
        completionOnPasteOpt.has_value()) {
        logger::info(format("Update completion on paste: {}", completionOnPasteOpt.value()));
        _configCompletionOnPaste.store(completionOnPasteOpt.value());
    }
    if (const auto debounceDelayOpt = completionConfig.debounceDelay;
        debounceDelayOpt.has_value()) {
        logger::info(format("Update debounce delay: {}ms", debounceDelayOpt.value()));
        _configDebounceDelay.store(debounceDelayOpt.value());
    }
    if (const auto pasteMaxLineCountOpt = completionConfig.pasteMaxLineCount;
        pasteMaxLineCountOpt.has_value()) {
        logger::info(format("Update paste max line count: {}", pasteMaxLineCountOpt.value()));
        _configPasteMaxLineCount.store(pasteMaxLineCountOpt.value());
    }
    if (const auto prefixLineCountOpt = completionConfig.prefixLineCount;
        prefixLineCountOpt.has_value()) {
        logger::info(format("Update prefix line count: {}", prefixLineCountOpt.value()));
        _configPrefixLineCount.store(prefixLineCountOpt.value());
    }
    if (const auto recentFileCountOpt = completionConfig.recentFileCount;
        recentFileCountOpt.has_value()) {
        logger::info(format("Update recent file count: {}", recentFileCountOpt.value()));
        _configRecentFileCount.store(recentFileCountOpt.value());
    }
    if (const auto suffixLineCountOpt = completionConfig.suffixLineCount;
        suffixLineCountOpt.has_value()) {
        logger::info(format("Update suffix line count: {}", suffixLineCountOpt.value()));
        _configSuffixLineCount.store(suffixLineCountOpt.value());
    }
}

void CompletionManager::wsCompletionGenerate(nlohmann::json&& data) {
    if (const auto serverMessage = CompletionGenerateServerMessage(move(data));
        serverMessage.result == "success") {
        const auto completions = serverMessage.completions().value();
        if (completions.empty()) {
            logger::log("(WsAction::CompletionGenerate) Ignore due to empty completions");
            return;
        }
        const auto& actionId = completions.actionId;
        if (_needDiscardWsAction.load()) {
            logger::log("(WsAction::CompletionGenerate) Ignore due to debounce");
            WebsocketManager::GetInstance()->send(CompletionCancelClientMessage(actionId, false));
            return;
        }
        const auto [candidate, index] = completions.current(); {
            unique_lock lock(_completionsMutex);
            _completionsOpt.emplace(completions);
        } {
            unique_lock lock(_completionCacheMutex);
            _completionCache.reset(iconv::autoEncode(candidate));
        }
        if (const auto currentWindowHandleOpt = WindowManager::GetInstance()->getCurrentWindowHandle();
            currentWindowHandleOpt.has_value()) {
            unique_lock lock(_editedCompletionMapMutex);
            _editedCompletionMap.emplace(
                actionId,
                EditedCompletion{
                    actionId,
                    currentWindowHandleOpt.value(),
                    MemoryManipulator::GetInstance()->getCaretPosition().line,
                    candidate
                }
            );
        }

        const auto interactionLock = InteractionMonitor::GetInstance()->getInteractionLock();
        const auto [height, xPosition,yPosition] = common::getCaretDimensions();
        WebsocketManager::GetInstance()->send(CompletionSelectClientMessage(
            completions.actionId,
            completions.generateType,
            index,
            height,
            xPosition,
            yPosition
        ));
    } else {
        logger::warn(format(
            "(WsAction::CompletionGenerate) Result: {}\n"
            "\tMessage: {}",
            serverMessage.result,
            serverMessage.message()
        ));
    }
}

bool CompletionManager::_cancelCompletion() {
    bool hasCompletion;
    optional<Completions> completionsOpt; {
        shared_lock lock(_completionsMutex);
        if (_completionsOpt.has_value()) {
            completionsOpt.emplace(_completionsOpt.value());
        }
    } {
        unique_lock lock(_completionCacheMutex);
        const auto [oldContent, _] = _completionCache.reset();
        hasCompletion = !oldContent.empty();
    }
    if (completionsOpt.has_value()) {
        WebsocketManager::GetInstance()->send(
            CompletionCancelClientMessage(completionsOpt.value().actionId, true)
        );
        unique_lock lock(_editedCompletionMapMutex);
        if (_editedCompletionMap.contains(completionsOpt.value().actionId)) {
            _editedCompletionMap.at(completionsOpt.value().actionId).react(false);
        }
    }
    return hasCompletion;
}

void CompletionManager::_sendGenerateMessage(const CompletionComponents& completionComponents) {
    auto currentPath = completionComponents.path;
    bool isSameFilePath; {
        shared_lock lock(_lastEditedFilePathMutex);
        isSameFilePath = _lastEditedFilePath == currentPath;
    }
    if (!isSameFilePath) {
        SymbolManager::GetInstance()->updateRootPath(currentPath);
        unique_lock lock(_lastEditedFilePathMutex);
        _lastEditedFilePath = move(currentPath);
    }

    _needDiscardWsAction.store(false);
    WebsocketManager::GetInstance()->send(CompletionGenerateClientMessage(completionComponents));
    logger::info("Generate 'common' completion");
}

vector<filesystem::path> CompletionManager::_getRecentFiles() const {
    shared_lock lock(_recentFilesMutex);
    return _recentFiles | views::keys | ranges::to<vector>();
}

optional<CompletionComponents> CompletionManager::_retrieveContext(
    const CaretPosition& caretPosition,
    const CompletionComponents::GenerateType generateType,
    const string& infix
) const {
    const auto memoryManipulator = MemoryManipulator::GetInstance();
    const auto currentFileHandle = memoryManipulator->getHandle(MemoryAddress::HandleType::File);
    const auto currentLineCount = memoryManipulator->getCurrentLineCount();

    const auto interactionLock = InteractionMonitor::GetInstance()->getInteractionLock();
    if (const auto path = memoryManipulator->getCurrentFilePath();
        currentFileHandle && currentLineCount && !path.empty()) {
        CompletionComponents completionComponents(generateType, caretPosition, path);
        string prefix, prefixForSymbol, suffix; {
            const auto currentLine = memoryManipulator->getLineContent(
                currentFileHandle, caretPosition.line
            );
            prefix = iconv::autoDecode(currentLine.substr(0, caretPosition.character));
            suffix = iconv::autoDecode(currentLine.substr(caretPosition.character));
        }
        for (
            uint32_t index = 1;
            index <= min(caretPosition.line, _configPrefixLineCount.load());
            ++index
        ) {
            const auto tempLine = iconv::autoDecode(memoryManipulator->getLineContent(
                currentFileHandle, caretPosition.line - index
            )).append("\n");
            prefix.insert(0, tempLine);
            if (prefixForSymbol.empty() && regex_search(tempLine, regex(R"~(^\/\/.*|^\/\*\*.*)~"))) {
                prefixForSymbol = prefix;
            }
        }
        for (uint32_t index = 1; index < _configSuffixLineCount.load(); ++index) {
            const auto tempLine = iconv::autoDecode(
                memoryManipulator->getLineContent(currentFileHandle, caretPosition.line + index)
            );
            suffix.append("\n").append(tempLine);
        }
        completionComponents.setContext(prefix, infix, suffix);
        completionComponents.setRecentFiles(_getRecentFiles());
        completionComponents.setSymbols(
            SymbolManager::GetInstance()->getSymbols(prefixForSymbol, path)
        );

        return completionComponents;
    }
    return nullopt;
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

void CompletionManager::_threadCheckAcceptedCompletions() {
    thread([this] {
        while (_isRunning) {
            vector<EditedCompletion> needReportCompletions{}; {
                const shared_lock lock(_editedCompletionMapMutex);
                for (const auto& editedCompletion: _editedCompletionMap | views::values) {
                    if (editedCompletion.canReport()) {
                        needReportCompletions.push_back(editedCompletion);
                    }
                }
            } {
                const auto interactionLock = InteractionMonitor::GetInstance()->getInteractionLock();
                const unique_lock editedCompletionMapLock(_editedCompletionMapMutex);
                for (const auto& needReportCompletion: needReportCompletions) {
                    _editedCompletionMap.erase(needReportCompletion.actionId);
                    WebsocketManager::GetInstance()->send(needReportCompletion.parse());
                }
            }
            this_thread::sleep_for(1s);
        }
    }).detach();
}

void CompletionManager::_threadCheckCurrentFilePath() {
    thread([this] {
        while (_isRunning) {
            filesystem::path currentPath; {
                const auto interactionLock = InteractionMonitor::GetInstance()->getInteractionLock();
                currentPath = MemoryManipulator::GetInstance()->getCurrentFilePath();
            }
            if (const auto extension = currentPath.extension();
                extension == ".c" || extension == ".h") {
                unique_lock lock(_recentFilesMutex);
                _recentFiles.emplace_back(currentPath, chrono::high_resolution_clock::now());
                ranges::push_heap(_recentFiles, fileTimeCompare);
                if (_recentFiles.size() > _configRecentFileCount.load()) {
                    ranges::pop_heap(_recentFiles, fileTimeCompare);
                    _recentFiles.pop_front();
                }
            }
            this_thread::sleep_for(3s);
        }
    }).detach();
}

void CompletionManager::_threadDebounceRetrieveCompletion() {
    thread([this] {
        while (_isRunning) {
            if (const auto pastTime = chrono::high_resolution_clock::now() - _debounceRetrieveCompletionTime.load();
                pastTime >= chrono::milliseconds(_configDebounceDelay.load()) && _needRetrieveCompletion.load()) {
                try {
                    const auto memoryManipulator = MemoryManipulator::GetInstance();
                    const auto caretPosition = memoryManipulator->getCaretPosition();
                    if (const auto completionComponentsOpt = _retrieveContext(
                        caretPosition,
                        CompletionComponents::GenerateType::Common
                    ); completionComponentsOpt.has_value()) {
                        _sendGenerateMessage(completionComponentsOpt.value());
                        logger::info("Generate common completion");
                    }
                } catch (const exception& e) {
                    logger::warn(format("(_threadDebounceRetrieveCompletion) Exception: {}", e.what()));
                }
                _needRetrieveCompletion.store(false);
            }
            this_thread::sleep_for(10ms);
        }
    }).detach();
}
