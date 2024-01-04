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
#include <types/MemoryAddress.h>
#include <types/Range.h>

namespace components {
    class InteractionMonitor : public SingletonDclp<InteractionMonitor> {
    public:
        using CallBack = std::function<void(const std::any&)>;

        InteractionMonitor();

        ~InteractionMonitor() override;

        [[nodiscard]] std::tuple<int, int> getCaretPixels(int line) const;

        [[nodiscard]] std::string getFileName() const;

        [[nodiscard]] std::string getLineContent(uint32_t line) const;

        void setLineContent(uint32_t line, const std::string& content) const;

        template<class T>
        void registerInteraction(
            const types::Interaction interaction,
            T* const other,
            void (T::* const memberFunction)(const std::any&)
        ) {
            _handlerMap[interaction].push_back(std::bind_front(memberFunction, other));
        }

    private:
        const std::string _subKey;
        const uint32_t _baseAddress;
        helpers::KeyHelper _keyHelper;
        std::atomic<bool> _isRunning{true}, isLMDown{false}, _isSelecting{false};
        std::atomic<types::CaretPosition> _currentCaretPosition, _downCursorPosition;
        std::atomic<std::optional<types::Key>> _navigateBuffer;
        std::shared_ptr<void> _mouseHookHandle, _processHandle, _windowHookHandle;
        std::unordered_map<types::Interaction, std::vector<CallBack>> _handlerMap;
        types::MemoryAddress _memoryAddress{};

        void _handleKeycode(types::Keycode keycode) noexcept;

        void _handleInteraction(types::Interaction interaction, const std::any& data = {}) const noexcept;

        void _monitorAutoCompletion() const;

        void _monitorCaretPosition();

        types::Range _monitorCursorSelect() const;

        void _monitorDebugLog() const;

        void _monitorEditorInfo() const;

        void _processWindowMessage(long lParam);

        void _processWindowMouse(unsigned int wParam);

        void _retrieveProjectId(const std::string& project) const;

        static long __stdcall _windowProcedureHook(int nCode, unsigned int wParam, long lParam);
    };
}
