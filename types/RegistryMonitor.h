#pragma once

#include <chrono>

#include <singleton_dclp.hpp>

#include <types/CompletionCache.h>
#include <types/CursorPosition.h>
#include <types/UserAction.h>

namespace types {
    class RegistryMonitor : public SingletonDclp<RegistryMonitor> {
    public:
        RegistryMonitor();

        ~RegistryMonitor() override;

        void acceptByTab(unsigned int);

        void cancelByCursorNavigate(CursorPosition, CursorPosition);

        void cancelByDeleteBackward(CursorPosition oldPosition, CursorPosition newPosition);

        void cancelByKeycodeNavigate(unsigned int);

        void cancelByModifyLine(unsigned int keycode);

        void cancelBySave();

        void cancelByUndo();

        void retrieveEditorInfo(unsigned int keycode);

    private:
        const std::string _subKey = R"(SOFTWARE\Source Dynamics\Source Insight\3.0)";
        std::string _projectId, _projectHash;
        CompletionCache _completionCache;
        std::atomic<bool> _isRunning = true, _justInserted = false;
        std::atomic<std::chrono::time_point<std::chrono::high_resolution_clock>> _lastTriggerTime;

        void _cancelCompletion(UserAction action = UserAction::DeleteBackward, bool resetCache = true);

        void _insertCompletion(const std::string &data);

        void _reactToCompletion(CompletionCache::Completion&& completion);

        void _retrieveCompletion(const std::string &editorInfoString);

        void _retrieveProjectId(const std::string &projectFolder);
    };
}
