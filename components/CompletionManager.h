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

        CompletionManager() = default;

        void interactionAcceptCompletion(const std::any&);

        void interactionCaretUpdate(const std::any& data);

        void interactionDeleteInput(const std::any&);

        void interactionEnterInput(const std::any&);

        void interactionNavigate(const std::any& data);

        void interactionNormalInput(const std::any& data);

        void interactionSave(const std::any&);

        void instantUndo(const std::any& = {});

        void wsActionCompletionGenerate(const nlohmann::json& data);

        // TODO: Remove old methods

        void retrieveWithCurrentPrefix(const std::string& currentPrefix);

        void retrieveWithFullInfo(Components&& components);

        void setAutoCompletion(bool isAutoCompletion);

        void setProjectId(const std::string& projectId);

        void setVersion(const std::string& version);

    private:
        mutable std::shared_mutex _componentsMutex, _editorInfoMutex;
        Components _components;
        EditorInfo _editorInfo;

        // TODO: Check if _isJustAccepted is still needed
        std::atomic<bool> _isAutoCompletion{true}, _isContinuousEnter{false}, _isJustAccepted{false}, _isNewLine{true};
        std::atomic<types::Time> _currentRetrieveTime;
        types::CompletionCache _completionCache;

        void _cancelCompletion(bool isNeedReset = true);

        void _retrieveCompletion(const std::string& prefix);

        void _retrieveEditorInfo() const;

        void _updateComponents();
    };
}
