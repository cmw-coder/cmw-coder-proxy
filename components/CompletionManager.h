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
            std::string cursorString;
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

        std::optional<std::string> acceptCompletion();

        void cancelCompletion();

        void deleteInput(const types::CaretPosition& position);

        void normalInput(char character);

        // TODO: Remove old methods

        void instantUndo(const std::any& = {});

        void onCompletionGenerate(const nlohmann::json& data);

        void retrieveWithCurrentPrefix(const std::string& currentPrefix);

        void retrieveWithFullInfo(Components&& components);

        void setAutoCompletion(bool isAutoCompletion);

        void setProjectId(const std::string& projectId);

        void setVersion(const std::string& version);

    private:
        mutable std::shared_mutex _componentsMutex, _editorInfoMutex;
        Components _components;
        EditorInfo _editorInfo;
        std::atomic<bool> _isAutoCompletion{true}, _isContinuousEnter{}, _isJustAccepted{};
        std::atomic<types::Time> _currentRetrieveTime;
        types::CompletionCache _completionCache;

        void _cancelCompletion(bool isNeedReset = true);

        void _reactToCompletion(types::Completion&& completion, bool isAccept);

        void _retrieveCompletion(const std::string& prefix);

        void _retrieveEditorInfo() const;
    };
}
