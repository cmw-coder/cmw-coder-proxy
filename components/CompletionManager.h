#pragma once

#include <singleton_dclp.hpp>

#include <helpers/HttpHelper.h>
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

        ~CompletionManager() override;

        void acceptCompletion(const std::any&);

        void cancelCompletion(const std::any&data);

        void deleteInput(const std::any&data);

        void retrieveWithFullInfo(Components&&components);

        void retrieveWithCurrentPrefix(const std::string&currentPrefix);

        void setAutoCompletion(bool isAutoCompletion);

        void setProjectId(const std::string&projectId);

        void setVersion(const std::string&version);

    private:
        mutable std::shared_mutex _componentsMutex, _editorInfoMutex;
        Components _components;
        EditorInfo _editorInfo;
        helpers::HttpHelper _httpHelper;
        std::atomic<bool> _isAutoCompletion{true}, _isContinuousEnter{}, _isJustInserted{}, _isNeedInsert{};
        std::atomic<std::chrono::time_point<std::chrono::high_resolution_clock>> _currentRetrieveTime;
        types::CompletionCache _completionCache;

        void _cancelCompletion(bool isCrossLine = false, bool isNeedReset = true);

        void _reactToCompletion(types::Completion&&completion, bool isAccept);

        void _retrieveCompletion(const std::string&prefix);
    };
}
