#pragma once

#include <any>
#include <functional>
#include <queue>
#include <unordered_map>

#include <singleton_dclp.hpp>

#include <helpers/KeyHelper.h>
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

        std::shared_lock<std::shared_mutex> getInteractionLock() const;

        template<class T>
        void registerInteraction(
            const types::Interaction interaction,
            T* const other,
            void (T::* const memberFunction)(const std::any&, bool&)
        ) {
            _handlerMap[interaction].push_back(std::bind_front(memberFunction, other));
        }

    private:
        const helpers::KeyHelper _keyHelper;
        mutable std::shared_mutex _interactionMutex;
        std::atomic<bool> _isRunning{true}, _isMouseLeftDown{false}, _isSelecting{false},
                _needReleaseInteractionLock{false};
        std::atomic<types::CaretPosition> _currentCaretPosition, _downCursorPosition;
        std::atomic<std::optional<types::Key>> _navigateWithKey;
        std::atomic<std::optional<types::Mouse>> _navigateWithMouse;
        std::atomic<types::Time> _releaseInteractionLockTime;
        std::shared_ptr<void> _cbtHookHandle, _keyHookHandle, _mouseHookHandle, _processHandle, _windowHookHandle;
        std::unordered_map<types::Interaction, std::vector<InteractionCallBack>> _handlerMap;

        static long __stdcall _cbtProcedureHook(int nCode, unsigned int wParam, long lParam);

        static long __stdcall _keyProcedureHook(int nCode, unsigned int wParam, long lParam);

        static long __stdcall _mouseProcedureHook(int nCode, unsigned int wParam, long lParam);

        static long __stdcall _windowProcedureHook(int nCode, unsigned int wParam, long lParam);

        void _handleKeycode(types::Keycode keycode) noexcept;

        bool _handleInteraction(types::Interaction interaction, const std::any& data = {}) const noexcept;

        bool _processKeyMessage(unsigned wParam, unsigned lParam);

        void _processMouseMessage(unsigned wParam);

        void _processWindowMessage(long lParam);

        void _retrieveProjectId(const std::string& project) const;

        void _threadMonitorCaretPosition();

        void _threadReleaseInteractionLock();
    };
}
