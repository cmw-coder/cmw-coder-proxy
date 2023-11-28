#pragma once

#include <any>

#include <singleton_dclp.hpp>

#include <helpers/HttpHelper.h>
#include <types/common.h>
#include <types/CompletionCache.h>

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

        CompletionManager();

        void interactionAccept(const std::any& = {});

        void interactionCancel(const std::any&data);

        void interactionDelete(const std::any&data);

        void interactionEnter(const std::any& = {});

        void interactionNavigate(const std::any& = {});

        void interactionNormal(const std::any&data);

        void interactionSave(const std::any& = {});

        void interactionUndo(const std::any& = {});

        void retrieveWithCurrentPrefix(const std::string&currentPrefix);

        void retrieveWithFullInfo(Components&&components);

        void setAutoCompletion(bool isAutoCompletion);

        void setProjectId(const std::string&projectId);

        void setVersion(const std::string&version);

    private:
        mutable std::shared_mutex _componentsMutex, _editorInfoMutex;
        Components _components;
        EditorInfo _editorInfo;
        helpers::HttpHelper _httpHelper;
        std::atomic<bool> _isAutoCompletion{true}, _isContinuousEnter{}, _isJustAccepted{};
        std::atomic<types::Time> _currentRetrieveTime;
        types::CompletionCache _completionCache;

        void _cancelCompletion(bool isCrossLine = false, bool isNeedReset = true);

        void _reactToCompletion(types::Completion&&completion, bool isAccept);

        void _retrieveCompletion(const std::string&prefix);

        void _retrieveEditorInfo() const;
    };
}
