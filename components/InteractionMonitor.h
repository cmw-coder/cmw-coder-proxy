#pragma once

#include <any>
#include <functional>
#include <unordered_map>

#include <singleton_dclp.hpp>

#include <helpers/KeyHelper.h>
#include <types/common.h>
#include <types/CursorPosition.h>
#include <types/Interaction.h>

namespace components {
    class InteractionMonitor : public SingletonDclp<InteractionMonitor> {
    public:
        using CallBackFunction = std::function<void(std::any)>;

        InteractionMonitor();

        ~InteractionMonitor() override;

    private:
        helpers::KeyHelper _keyHelper;
        std::atomic<bool> _isRunning = true;
        std::atomic<types::CursorPosition> _cursorPosition;
        std::shared_ptr<void> _processHandle, _windowHookHandle;
        std::unordered_map<types::Interaction, CallBackFunction> _handlers;
        uint32_t _cursorLineAddress, _cursorCharAddress;

        types::CursorPosition _getCursorPosition() const;

        void _handleKeycode(types::Keycode keycode) const noexcept;

        void _processWindowMessage(long lParam) const;

        void _threadMonitorCursorPosition();

        static long __stdcall _windowProcedureHook(int nCode, unsigned int wParam, long lParam);
    };
}
