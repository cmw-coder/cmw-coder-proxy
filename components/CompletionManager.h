#pragma once

#include <any>
#include <deque>

#include <singleton_dclp.hpp>

#include <models/SymbolInfo.h>
#include <types/CaretPosition.h>
#include <types/common.h>
#include <types/Completions.h>
#include <types/CompletionCache.h>
#include <types/EditedCompletion.h>

namespace components {
    class CompletionManager : public SingletonDclp<CompletionManager> {
    public:
        using FileTime = std::pair<std::filesystem::path, std::chrono::high_resolution_clock::time_point>;

        CompletionManager();

        ~CompletionManager() override;

        void interactionCompletionAccept(const std::any&, bool& needBlockMessage);

        void interactionCompletionCancel(const std::any& data, bool& needBlockMessage);

        void interactionDeleteInput(const std::any&, bool&);

        void interactionEnterInput(const std::any&, bool&);

        void interactionNavigateWithKey(const std::any&, bool&);

        void interactionNavigateWithMouse(const std::any& data, bool&);

        void interactionNormalInput(const std::any& data, bool&);

        void interactionPaste(const std::any&, bool&);

        void interactionSave(const std::any&, bool&);

        void interactionUndo(const std::any&, bool&);

        void updateCompletionConfig(const models::CompletionConfig& completionConfig);

        void wsCompletionGenerate(nlohmann::json&& data);

    private:
        mutable std::shared_mutex _completionsMutex, _completionCacheMutex, _currentFilePathMutex,
                _lastCaretPositionMutex, _lastCompletionComponentsMutex, _lastEditedFilePathMutex, _recentFilesMutex;
        types::CaretPosition _lastCaretPosition{};
        std::atomic<bool> _isRunning{true}, _needDiscardWsAction{false},
                _needRetrieveCompletion{false};
        std::atomic<std::chrono::milliseconds> _configDebounceDelay{std::chrono::milliseconds(50)};
        std::atomic<types::Time> _debounceRetrieveCompletionTime;
        std::atomic<uint32_t> _configPasteFixMaxTriggerLineCount{10}, _configPrefixLineCount{200},
                _configRecentFileCount{5}, _configSuffixLineCount{80};
        std::deque<FileTime> _recentFiles;
        std::filesystem::path _currentFilePath, _lastEditedFilePath;
        std::optional<types::CompletionComponents> _lastCompletionComponents;
        std::optional<types::Completions> _completionsOpt;
        types::CompletionCache _completionCache;

        bool _cancelCompletion();

        std::vector<std::filesystem::path> _getRecentFiles() const;

        std::optional<types::CompletionComponents> _retrieveCompletionComponents(
            const types::CaretPosition& caretPosition,
            types::CompletionComponents::GenerateType generateType,
            const std::string& infix = ""
        ) const;

        void _sendGenerateMessage(const types::CompletionComponents& completionComponents);

        bool _hasValidCache() const;

        void _prolongRetrieveCompletion();

        void _updateNeedRetrieveCompletion(bool need = true, char character = 0);

        void _threadMonitorCurrentFilePath();

        void _threadDebounceRetrieveCompletion();
    };
}
