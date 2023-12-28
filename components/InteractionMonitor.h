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
#include <types/Range.h>

namespace components {
    class InteractionMonitor : public SingletonDclp<InteractionMonitor> {
    public:
        using InstantCallBack = std::function<void(const std::any&)>;

        InteractionMonitor();

        ~InteractionMonitor() override;

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
        helpers::KeyHelper _keyHelper;
        std::atomic<bool> _isRunning{true}, isLMDown{false}, _isSelecting{false};
        std::atomic<types::CaretPosition> _currentCursorPosition, _downCursorPosition;
        std::atomic<std::optional<types::Key>> _navigateBuffer;
        std::shared_ptr<void> _mouseHookHandle, _processHandle, _windowHookHandle;
        std::unordered_map<types::Interaction, std::vector<InstantCallBack>> _instantHandlers;
        uint32_t _cursorLineAddress, _cursorCharAddress, _cursorStartLineAddress, _cursorStartCharAddress,
                _cursorEndLineAddress, _cursorEndCharAddress;

        void _handleKeycode(types::Keycode keycode) noexcept;

        void _handleInteraction(types::Interaction interaction, const std::any& data = {}) const noexcept;

        void _monitorAutoCompletion() const;

        void _monitorCursorPosition();

        types::Range _monitorCursorSelect() const;

        void _monitorDebugLog() const;

        void _monitorEditorInfo() const;

        void _processWindowMessage(long lParam);

        void _processWindowMouse(unsigned int wParam);

        void _retrieveProjectId(const std::string& project) const;

        static long __stdcall _windowProcedureHook(int nCode, unsigned int wParam, long lParam);
    };
}
