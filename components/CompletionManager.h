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

        std::optional<std::string> acceptCompletion(int line);

        void cancelCompletion();

        void deleteInput(const types::CaretPosition& position);

        /**
         * @brief Handles normal input and returns if need to trigger accept.
         * @param character The character to be input.
         * @return A boolean indicating whether the input was handled successfully.
         */
        bool normalInput(char character);

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
        std::atomic<bool> _isAutoCompletion{true}, _isContinuousEnter{false}, _isJustAccepted{false};
        std::atomic<types::Time> _currentRetrieveTime;
        types::CompletionCache _completionCache;

        void _cancelCompletion(bool isNeedReset = true);

        void _retrieveCompletion(const std::string& prefix);

        void _retrieveEditorInfo() const;
    };
}
