#pragma once

#include <any>

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
        struct Components {
            types::CaretPosition caretPosition;
            std::filesystem::path path;
            std::string prefix;
            std::vector<std::filesystem::path> recentFiles;
            std::string suffix;
            std::vector<models::SymbolInfo> symbols;
        };

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

        void wsCompletionGenerate(nlohmann::json&& data);

    private:
        mutable std::shared_mutex _completionsMutex, _completionCacheMutex, _componentsMutex,
                _editedCompletionMapMutex, _modifyingFilesMutex;
        Components _components;
        std::atomic<bool> _isRunning{true},
                _needDiscardWsAction{false}, _needRetrieveCompletion{false};
        std::atomic<types::Time> _debounceRetrieveCompletionTime;
        std::optional<types::Completions> _completionsOpt;
        std::unordered_map<std::filesystem::path, std::chrono::high_resolution_clock::time_point> _recentFiles;
        std::unordered_map<std::string, types::EditedCompletion> _editedCompletionMap;
        types::CompletionCache _completionCache;

        bool _cancelCompletion();

        std::vector<std::filesystem::path> _getRecentFiles(uint32_t limit = 5) const;

        bool _hasValidCache() const;

        void _prolongRetrieveCompletion();

        void _updateNeedRetrieveCompletion(bool need = true, char character = 0);

        void _sendCompletionGenerate(
            int64_t completionStartTime,
            int64_t symbolStartTime,
            int64_t completionEndTime
        );

        void _threadCheckAcceptedCompletions();

        void _threadCheckCurrentFilePath();

        void _threadDebounceRetrieveCompletion();
    };
}
