#pragma once

#include <any>
#include <functional>
#include <queue>
#include <unordered_map>

#include <singleton_dclp.hpp>

#include <models/configs.h>
#include <types/common.h>
#include <types/CaretPosition.h>
#include <types/Interaction.h>
#include <types/Mouse.h>

namespace components {
    class InteractionMonitor : public SingletonDclp<InteractionMonitor> {
    public:
        using InteractionCallBack = std::function<void(const std::any&, bool&)>;

        InteractionMonitor();

        ~InteractionMonitor() override;

        std::unique_lock<std::shared_mutex> getInteractionLock() const;

        template<class T>
        void registerInteraction(
            const types::Interaction interaction,
            T* const other,
            void (T::* const memberFunction)(const std::any&, bool&)
        ) {
            _handlerMap[interaction].push_back(std::bind_front(memberFunction, other));
        }

        void updateCompletionConfig(const models::CompletionConfig& completionConfig);

        void updateShortcutConfig(const models::ShortcutConfig& shortcutConfig);

    private:
        mutable std::shared_mutex _configCommitMutex, _configManualCompletionMutex, _interactionMutex;
        std::atomic<bool> _isRunning{true}, _isSelecting{false}, _needUnlockInteraction{false};
        std::atomic<int32_t> _selectionLineCount{};
        std::atomic<std::optional<types::Mouse>> _navigateWithMouse;
        std::atomic<types::CaretPosition> _currentCaretPosition, _downCursorPosition;
        std::atomic<types::Time> _interactionUnlockTime;
        std::atomic<uint32_t> _navigateKeycode{0}, _interactionUnlockDelay;
        std::shared_ptr<void> _cbtHookHandle, _keyHookHandle, _mouseHookHandle, _processHandle, _windowHookHandle;
        std::unordered_map<types::Interaction, std::vector<InteractionCallBack>> _handlerMap;
        types::KeyCombination _configCommit, _configManualCompletion;

        static long __stdcall _cbtProcedureHook(int nCode, unsigned int wParam, long lParam);

        static long __stdcall _keyProcedureHook(int nCode, unsigned int wParam, long lParam);

        static long __stdcall _mouseProcedureHook(int nCode, unsigned int wParam, long lParam);

        static long __stdcall _windowProcedureHook(int nCode, unsigned int wParam, long lParam);

        bool _handleInteraction(types::Interaction interaction, const std::any& data = {}) const noexcept;

        void _handleMouseButtonUp();

        void _handleSelectionReplace(int32_t offsetLineCount = 0);

        void _interactionLockShared();

        bool _processKeyMessage(uint32_t virtualKeyCode, uint32_t lParam);

        void _processMouseMessage(unsigned wParam);

        void _processWindowMessage(long lParam);

        void _retrieveProjectId(const std::string& project) const;

        void _threadMonitorCaretPosition();

        void _threadReleaseInteractionLock();
    };
}
