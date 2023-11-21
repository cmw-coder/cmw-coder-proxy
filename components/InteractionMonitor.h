#pragma once

#include <any>
#include <functional>
#include <optional>
#include <unordered_map>

#include <types/CursorPosition.h>
#include <types/Interaction.h>

#include <singleton_dclp.hpp>

namespace components {
    class InteractionMonitor : public SingletonDclp<InteractionMonitor> {
    public:
        using CallBackFunction = std::function<void(std::optional<std::any>)>;

        InteractionMonitor();

        ~InteractionMonitor() override;

    private:
        std::atomic<types::CursorPosition> _cursorPosition;
        std::atomic<int64_t> _codeWindowHandle = -1, _popListWindowHandle = -1;
        std::shared_ptr<void> _processHandle, _windowHookHandle;
        std::unordered_map<types::Interaction, CallBackFunction> _handlers;

        void _processWindowMessage(long lParam);

        void _threadMonitorCursorPosition();

        static long __stdcall _windowProcedureHook(int nCode, unsigned int wParam, long lParam);
    };
}
