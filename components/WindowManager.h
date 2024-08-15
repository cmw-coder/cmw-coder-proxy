#pragma once

#include <any>

#include <singleton_dclp.hpp>

#include <types/common.h>

namespace components {
    class WindowManager : public SingletonDclp<WindowManager> {
    public:
        WindowManager();

        ~WindowManager() override;

        bool checkNeedHideWhenLostFocus(uint32_t windowHandle);

        bool checkNeedShowWhenGainFocus(uint32_t windowHandle);

        void closeWindowHandle(uint32_t windowHandle);

        std::optional<uint32_t> getAssosiatedFileHandle(uint32_t windowHandle) const;

        std::tuple<int64_t, int64_t> getClientPosition() const;

        std::optional<uint32_t> getCurrentWindowHandle() const;

        bool hasPopListWindow() const;

        void interactionPaste(const std::any& = {});

        void sendF13() const;

        void sendLeftThenRight() const;

        bool sendSave();

        bool sendUndo();

        void setMenuText(const std::string& text) const;

        void unsetMenuText() const;

    private:
        mutable std::shared_mutex _fileHandleMapMutex;

        const std::string _menuBaseText = "Comware Coder Proxy: ";
        std::atomic<bool> _isRunning{true}, _needRetrieveInfo{false};
        std::atomic<int64_t> _menuHandle{-1}, _menuItemIndex{-1}, _popListWindowHandle{-1};
        std::atomic<std::optional<uint32_t>> _currentWindowHandle{};
        std::atomic<types::Time> _debounceRetrieveInfoTime;
        std::unordered_map<uint32_t, uint32_t> _fileHandleMap;

        void _addEditorWindowHandle(uint32_t windowHandle);

        void _cancelRetrieveInfo();

        void _threadInitMenuHandle();
    };
}
