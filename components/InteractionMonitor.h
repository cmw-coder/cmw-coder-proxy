#pragma once

#include <any>
#include <functional>
#include <queue>
#include <unordered_map>
#include <unordered_set>

#include <singleton_dclp.hpp>

#include <helpers/KeyHelper.h>
#include <types/common.h>
#include <types/CaretPosition.h>
#include <types/Interaction.h>
#include <types/Modification.h>

namespace components {
    class InteractionMonitor : public SingletonDclp<InteractionMonitor> {
    public:
        using CallBackFunction = std::function<void(const std::any&)>;

        InteractionMonitor();

        ~InteractionMonitor() override;

        template<class T>
        void addHandler(
            const types::Interaction userAction,
            T* const other,
            void (T::* const memberFunction)(const std::any&)
        ) {
            _handlers[userAction].push_back(std::bind_front(memberFunction, other));
        }

    private:
        const std::string _subKey;
        mutable std::shared_mutex _interactionBufferMutex;
        helpers::KeyHelper _keyHelper;
        std::atomic<bool> _isRunning{true};
        std::atomic<types::CaretPosition> _currentCursorPosition;
        std::queue<types::Modification> _modificationQueue;
        std::shared_ptr<void> _processHandle, _windowHookHandle;
        std::unordered_map<types::Interaction, std::vector<CallBackFunction>> _handlers;
        std::unordered_set<types::Interaction> _interactionBuffer;
        uint32_t _cursorLineAddress, _cursorCharAddress;

        void _handleKeycode(types::Keycode keycode) noexcept;

        void _handleInteraction(types::Interaction interaction, const std::any&data = {}) const noexcept;

        void _monitorAutoCompletion() const;

        void _monitorCursorPosition();

        void _monitorDebugLog() const;

        void _monitorEditorInfo() const;

        void _processWindowMessage(long lParam);

        void _queueInteractionIntoBuffer(types::Interaction interaction);

        void _retrieveProjectId(const std::string&project) const;

        static long __stdcall _windowProcedureHook(int nCode, unsigned int wParam, long lParam);
    };
}
