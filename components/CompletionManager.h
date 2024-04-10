#pragma once

#include <any>

#include <singleton_dclp.hpp>

#include <helpers/HttpHelper.h>
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
            std::string path;
            std::string prefix;
            std::string project;
            std::vector<std::string> recentFiles;
            std::string suffix;
            std::vector<models::SymbolInfo> symbols;
        };

        CompletionManager();

        ~CompletionManager() override;

        void interactionCompletionAccept(const std::any&, bool& needBlockMessage);

        void interactionCompletionCancel(const std::any& data, bool&);

        void interactionDeleteInput(const std::any&, bool&);

        void interactionEnterInput(const std::any&, bool&);

        void interactionNavigateWithKey(const std::any& data, bool&);

        void interactionNavigateWithMouse(const std::any& data, bool&);

        void interactionNormalInput(const std::any& data, bool&);

        void interactionPaste(const std::any&, bool&);

        void interactionSave(const std::any&, bool&);

        void interactionUndo(const std::any&, bool&);

        void wsCompletionGenerate(nlohmann::json&& data);

    private:
        mutable std::shared_mutex _editedCompletionMapMutex, _completionsMutex, _completionCacheMutex, _componentsMutex;
        Components _components;
        std::atomic<bool> _isNewLine{true}, _isRunning{true},
                _needDiscardWsAction{false}, _needRetrieveCompletion{false};
        std::atomic<types::Time> _debounceRetrieveCompletionTime;
        std::optional<types::Completions> _completionsOpt;
        std::unordered_map<std::string, types::EditedCompletion> _editedCompletionMap;
        types::CompletionCache _completionCache;

        void _cancelCompletion();

        bool _hasValidCache() const;

        void _prolongRetrieveCompletion();

        void _updateNeedRetrieveCompletion(bool need = true, char character = 0);

        void _sendCompletionGenerate();

        void _threadCheckAcceptedCompletions();

        void _threadDebounceRetrieveCompletion();
    };
}
