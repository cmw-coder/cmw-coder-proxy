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
#include <types/Mouse.h>
#include <types/Range.h>

namespace components {
    class InteractionMonitor : public SingletonDclp<InteractionMonitor> {
    public:
        using InteractionCallBack = std::function<void(const std::any&, bool&)>;

        InteractionMonitor();

        ~InteractionMonitor() override;

        void deleteLineContent(uint32_t line) const;

        [[nodiscard]] std::tuple<int64_t, int64_t> getCaretPixels(uint32_t line) const;

        [[nodiscard]] types::CaretPosition getCaretPosition() const;

        [[nodiscard]] std::string getFileName() const;

        [[nodiscard]] std::string getLineContent() const;

        [[nodiscard]] std::string getLineContent(uint32_t line) const;

        [[nodiscard]] std::string getProjectDirectory() const;

        void insertLineContent(uint32_t line, const std::string& content) const;

        template<class T>
        void registerInteraction(
            const types::Interaction interaction,
            T* const other,
            void (T::* const memberFunction)(const std::any&, bool&)
        ) {
            _handlerMap[interaction].push_back(std::bind_front(memberFunction, other));
        }

        void setCaretPosition(const types::CaretPosition& caretPosition) const;

        void setLineContent(uint32_t line, const std::string& content) const;

        void setSelectedContent(const std::string& content) const;

    private:
        const std::string _subKey;
        const uint32_t _baseAddress;
        helpers::KeyHelper _keyHelper;
        std::atomic<bool> _isRunning{true}, _isMouseLeftDown{false}, _isSelecting{false};
        std::atomic<types::CaretPosition> _currentCaretPosition, _downCursorPosition;
        std::atomic<std::optional<types::Key>> _navigateWithKey;
        std::atomic<std::optional<types::Mouse>> _navigateWithMouse;
        std::shared_ptr<void> _keyHookHandle, _mouseHookHandle, _processHandle, _windowHookHandle;
        std::unordered_map<types::Interaction, std::vector<InteractionCallBack>> _handlerMap;
        types::MemoryAddress _memoryAddress{};

        static long __stdcall _keyProcedureHook(int nCode, unsigned int wParam, long lParam);

        static long __stdcall _mouseProcedureHook(int nCode, unsigned int wParam, long lParam);

        static long __stdcall _windowProcedureHook(int nCode, unsigned int wParam, long lParam);

        uint32_t _getFileHandle() const;

        uint32_t _getProjectHandle() const;

        uint32_t _getWindowHandle() const;

        void _handleKeycode(types::Keycode keycode) noexcept;

        bool _handleInteraction(types::Interaction interaction, const std::any& data = {}) const noexcept;

        void _monitorCaretPosition();

        types::Range _monitorCursorSelect() const;

        void _monitorDebugLog() const;

        bool _processKeyMessage(unsigned wParam, unsigned lParam);

        void _processMouseMessage(unsigned wParam);

        void _processWindowMessage(long lParam);

        void _retrieveProjectId(const std::string& project) const;
    };
}
