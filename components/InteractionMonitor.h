#pragma once

#include <any>
#include <functional>
#include <queue>
#include <unordered_map>

#include <singleton_dclp.hpp>

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

    private:
        mutable std::shared_mutex _interactionMutex;
        std::atomic<bool> _isRunning{true}, _isSelecting{false}, _needReleaseInteractionLock{false};
        std::atomic<types::CaretPosition> _currentCaretPosition, _downCursorPosition;
        std::atomic<uint32_t> _navigateKeycode{0};
        std::atomic<std::optional<types::Mouse>> _navigateWithMouse;
        std::atomic<types::Time> _releaseInteractionLockTime;
        std::shared_ptr<void> _cbtHookHandle, _keyHookHandle, _mouseHookHandle, _processHandle, _windowHookHandle;
        std::unordered_map<types::Interaction, std::vector<InteractionCallBack>> _handlerMap;

        static long __stdcall _cbtProcedureHook(int nCode, unsigned int wParam, long lParam);

        static long __stdcall _keyProcedureHook(int nCode, unsigned int wParam, long lParam);

        static long __stdcall _mouseProcedureHook(int nCode, unsigned int wParam, long lParam);

        static long __stdcall _windowProcedureHook(int nCode, unsigned int wParam, long lParam);

        bool _handleInteraction(types::Interaction interaction, const std::any& data = {}) const noexcept;

        void _handleMouseButtonUp();

        void _interactionLockShared();

        bool _processKeyMessage(uint32_t virtualKeyCode, uint32_t lParam);

        void _processMouseMessage(unsigned wParam);

        void _processWindowMessage(long lParam);

        void _retrieveProjectId(const std::string& project) const;

        void _threadMonitorCaretPosition();

        void _threadReleaseInteractionLock();
    };
}
