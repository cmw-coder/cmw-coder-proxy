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

        void interactionSelectionReplace(const std::any& data, bool&);

        void interactionUndo(const std::any&, bool&);

        void updateCompletionConfig(const models::CompletionConfig& completionConfig);

        void wsCompletionGenerate(nlohmann::json&& data);

    private:
        mutable std::shared_mutex _completionsMutex, _completionCacheMutex, _editedCompletionMapMutex,
                _lastCaretPositionMutex, _lastEditedFilePathMutex, _recentFilesMutex;
        types::CaretPosition _lastCaretPosition{};
        std::atomic<bool> _configCompletionOnPaste{true}, _isRunning{true}, _needDiscardWsAction{false},
                _needRetrieveCompletion{false};
        std::atomic<types::Time> _debounceRetrieveCompletionTime;
        std::atomic<uint32_t> _configDebounceDelay{50}, _configPasteMaxLineCount{10}, _configPrefixLineCount{200},
                _configRecentFileCount{5}, _configSuffixLineCount{80};
        std::filesystem::path _lastEditedFilePath;
        std::optional<types::Completions> _completionsOpt;
        std::deque<FileTime> _recentFiles;
        std::unordered_map<std::string, types::EditedCompletion> _editedCompletionMap;
        types::CompletionCache _completionCache;

        bool _cancelCompletion();

        std::vector<std::filesystem::path> _getRecentFiles() const;

        std::optional<types::CompletionComponents> _retrieveContext(
            const types::CaretPosition& caretPosition,
            types::CompletionComponents::GenerateType generateType,
            const std::string& infix = ""
        ) const;

        void _sendGenerateMessage(const types::CompletionComponents& completionComponents);

        bool _hasValidCache() const;

        void _prolongRetrieveCompletion();

        void _updateNeedRetrieveCompletion(bool need = true, char character = 0);

        void _threadCheckAcceptedCompletions();

        void _threadCheckCurrentFilePath();

        void _threadDebounceRetrieveCompletion();
    };
}
