#pragma once

#include <any>

#include <singleton_dclp.hpp>

#include <helpers/HttpHelper.h>
#include <types/CaretPosition.h>
#include <types/CompletionCache.h>
#include <types/common.h>

namespace components {
    class CompletionManager : public SingletonDclp<CompletionManager> {
    public:
        struct Components {
            types::CaretPosition caretPosition;
            std::string path;
            std::string prefix;
            std::string suffix;
            std::string symbolString;
            std::string tabString;
        };

        struct EditorInfo {
            std::string projectId;
            std::string version;
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

        void setProjectId(const std::string& projectId);

        void setVersion(const std::string& version);

    private:
        mutable std::shared_mutex _completionCacheMutex, _componentsMutex, _editorInfoMutex;
        Components _components;
        EditorInfo _editorInfo;

        // TODO: Check if _isJustAccepted is still needed
        std::atomic<bool> _isAutoCompletion{true}, _isContinuousEnter{false}, _isJustAccepted{false},
                _isNewLine{true}, _isRunning{true}, _needDiscardWsAction{false}, _needRetrieveCompletion{false};
        std::atomic<types::Time> _debounceRetrieveCompletionTime;
        types::CompletionCache _completionCache;

        void _cancelCompletion();

        bool _hasValidCache() const;

        void _prolongRetrieveCompletion();

        void _requestRetrieveCompletion();

        void _sendCompletionGenerate();

        void _threadDebounceRetrieveCompletion();
    };
}
