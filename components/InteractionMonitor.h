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
#include <types/Selection.h>

namespace components {
    class InteractionMonitor : public SingletonDclp<InteractionMonitor> {
    public:
        using Handler = std::function<void(const std::any&, bool&)>;

        InteractionMonitor();

        ~InteractionMonitor() override;

        std::unique_lock<std::shared_mutex> getInteractionLock() const;

        void registerInteraction(
            const types::Interaction interaction,
            Handler&& handler
        ) {
            _handlerMap[interaction].push_back(std::move(handler));
        }

        template<class T>
        void registerInteraction(
            const types::Interaction interaction,
            T* const other,
            void (T::* const memberFunction)(const std::any&, bool&)
        ) {
            _handlerMap[interaction].push_back(std::bind_front(memberFunction, other));
        }

        void updateGenericConfig(const models::GenericConfig& genericConfig);

        void updateShortcutConfig(const models::ShortcutConfig& shortcutConfig);

    private:
        mutable std::shared_mutex _configCommitMutex, _configManualCompletionMutex, _interactionMutex;
        std::atomic<bool> _isRunning{true}, _isSelecting{false}, _needUnlockInteraction{false};
        std::atomic<std::chrono::milliseconds> _configInteractionUnlockDelay{std::chrono::milliseconds(50)};
        std::atomic<std::chrono::seconds> _configAutoSaveInterval{std::chrono::seconds(300)};
        std::atomic<std::optional<types::Mouse>> _navigateWithMouse;
        std::atomic<types::CaretPosition> _currentCaretPosition, _downCursorPosition;
        std::atomic<types::Time> _interactionUnlockTime;
        std::atomic<uint32_t> _navigateKeycode{0};
        std::shared_ptr<void> _cbtHookHandle, _keyHookHandle, _mouseHookHandle, _processHandle, _windowHookHandle;
        std::unordered_map<types::Interaction, std::vector<Handler>> _handlerMap;
        types::KeyCombination _configCommit, _configManualCompletion;

        static long __stdcall _cbtProcedureHook(int nCode, unsigned int wParam, long lParam);

        static long __stdcall _keyProcedureHook(int nCode, unsigned int wParam, long lParam);

        static long __stdcall _mouseProcedureHook(int nCode, unsigned int wParam, long lParam);

        static long __stdcall _windowProcedureHook(int nCode, unsigned int wParam, long lParam);

        bool _handleInteraction(types::Interaction interaction, const std::any& data = {}) const noexcept;

        void _handleMouseButtonUp();

        void _handleSelectionReplace(const types::Selection& selection, int32_t offsetCount = 0) const;

        void _interactionLockShared();

        bool _processKeyMessage(uint32_t virtualKeyCode, uint32_t lParam);

        void _processMouseMessage(unsigned wParam);

        void _processWindowMessage(long lParam);

        void _retrieveProjectId(const std::string& project) const;

        void _threadAutoSave() const;

        void _threadMonitorCaretPosition();

        void _threadReleaseInteractionLock();
    };
}
