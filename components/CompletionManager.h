#pragma once

#include <any>

#include <singleton_dclp.hpp>

#include <helpers/HttpHelper.h>
#include <types/CaretPosition.h>
#include <types/common.h>
#include <types/CompletionCache.h>
#include <types/SymbolInfo.h>

namespace components {
    class CompletionManager : public SingletonDclp<CompletionManager> {
    public:
        struct Components {
            types::CaretPosition caretPosition;
            std::string path;
            std::string prefix;
            std::string project;
            std::string suffix;
            std::vector<std::string> recentFiles;
            std::vector<types::SymbolInfo> symbols;
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

        void wsActionCompletionGenerate(const nlohmann::json& data);

        // TODO: Remove old methods

        void setAutoCompletion(bool isAutoCompletion);
    private:
        mutable std::shared_mutex _completionCacheMutex, _completionListMutex, _componentsMutex;
        Components _components;

        // TODO: Check if _isJustAccepted is still needed
        std::atomic<bool> _isAutoCompletion{true}, _isContinuousEnter{false}, _isJustAccepted{false},
                _isNewLine{true}, _isRunning{true}, _needDiscardWsAction{false}, _needRetrieveCompletion{false};
        std::atomic<types::Time> _debounceRetrieveCompletionTime;
        std::vector<std::string> _completionList;
        types::CompletionCache _completionCache;

        void _cancelCompletion();

        bool _hasValidCache() const;

        void _prolongRetrieveCompletion();

        void _requestRetrieveCompletion();

        std::string _selectCompletion(uint32_t index = 0);

        void _sendCompletionGenerate();

        void _threadDebounceRetrieveCompletion();
    };
}
