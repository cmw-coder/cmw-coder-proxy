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

namespace components {
    class InteractionMonitor : public SingletonDclp<InteractionMonitor> {
    public:
        // new position, old position, data
        using DelayedCallBack = std::function<void(types::CaretPosition, types::CaretPosition, const std::any&)>;
        // data
        using InstantCallBack = std::function<void(const std::any&)>;

        InteractionMonitor();

        ~InteractionMonitor() override;

        template<class T>
        void addDelayedHandler(
            const types::Interaction userAction,
            T* const other,
            void (T::* const memberFunction)(types::CaretPosition, types::CaretPosition, const std::any&)
        ) {
            _delayedHandlers[userAction].push_back(std::bind_front(memberFunction, other));
        }

        template<class T>
        void addInstantHandler(
            const types::Interaction userAction,
            T* const other,
            void (T::* const memberFunction)(const std::any&)
        ) {
            _instantHandlers[userAction].push_back(std::bind_front(memberFunction, other));
        }

    private:
        const std::string _subKey;
        mutable std::shared_mutex _delayedInteractionsMutex, _modificationBufferMutex;
        helpers::KeyHelper _keyHelper;
        std::atomic<bool> _isRunning{true};
        std::atomic<types::CaretPosition> _currentCursorPosition;
        std::shared_ptr<void> _processHandle, _windowHookHandle;
        std::unordered_map<types::Interaction, std::vector<DelayedCallBack>> _delayedHandlers;
        std::unordered_map<types::Interaction, std::vector<InstantCallBack>> _instantHandlers;
        std::vector<std::pair<types::Interaction, std::any>> _delayedInteractions;
        uint32_t _cursorLineAddress, _cursorCharAddress;

        void _delayInteraction(types::Interaction interaction, std::any&& data = {});

        void _handleKeycode(types::Keycode keycode) noexcept;

        void _handleDelayedInteraction(
            types::Interaction interaction,
            types::CaretPosition newPosition,
            types::CaretPosition oldPosition,
            const std::any& data = {}
        ) const noexcept;

        void _handleInstantInteraction(types::Interaction interaction, const std::any& data = {}) const noexcept;

        void _monitorAutoCompletion() const;

        void _monitorCursorPosition();

        void _monitorDebugLog() const;

        void _monitorEditorInfo() const;

        void _processWindowMessage(long lParam);

        void _retrieveProjectId(const std::string& project) const;

        static long __stdcall _windowProcedureHook(int nCode, unsigned int wParam, long lParam);
    };
}
