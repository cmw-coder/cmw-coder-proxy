#pragma once

#include <chrono>

#include <singleton_dclp.hpp>

#include <types/common.h>
#include <types/CompletionCache.h>
#include <types/CursorPosition.h>
#include <types/UserAction.h>

namespace types {
    class RegistryMonitor : public SingletonDclp<RegistryMonitor> {
    public:
        RegistryMonitor();

        ~RegistryMonitor() override;

        void acceptByTab(Keycode);

        void cancelByCursorNavigate(CursorPosition, CursorPosition);

        void cancelByDeleteBackward(CursorPosition oldPosition, CursorPosition newPosition);

        void cancelByKeycodeNavigate(Keycode);

        void cancelByModifyLine(Keycode keycode);

        void cancelBySave();

        void cancelByUndo();

        void processNormalKey(Keycode keycode);

    private:
        const std::string _subKey;
        std::string _projectId, _projectHash, _pluginVersion;
        CompletionCache _completionCache;
        std::atomic<bool> _isAutoCompletion = true, _isRunning = true, _needInsert = false, _justInserted = false;
        std::atomic<std::chrono::time_point<std::chrono::high_resolution_clock>> _lastTriggerTime;

        void _cancelCompletion(UserAction action = UserAction::DeleteBackward, bool resetCache = true);

        void _insertCompletion(const std::string&data) const;

        void _reactToCompletion(Completion&&completion, bool isAccept);

        void _retrieveCompletion(const std::string&editorInfoString);

        void _retrieveEditorInfo() const;

        void _retrieveProjectId(const std::string&projectFolder);

        void _threadCompletionMode();

        void _threadLogDebug() const;

        void _threadProcessInfo();
    };
}
