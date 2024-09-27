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

        std::optional<uint32_t> getAssociatedFileHandle(uint32_t windowHandle) const;

        std::tuple<int64_t, int64_t> getClientPosition() const;

        std::tuple<int64_t, std::shared_lock<std::shared_mutex>> sharedAccessCodeWindowHandle() const;

        bool hasCodeWindow() const;

        bool hasPopListWindow() const;

        void sendEnd() const;

        void sendEscape() const;

        void sendF13() const;

        void sendLeftThenRight() const;

        bool sendSave() const;

        bool sendFocus() const;

        void setMenuText(const std::string& text) const;

        void unsetMenuText() const;

    private:
        mutable std::shared_mutex _codeWindowHandleMutex, _fileHandleMapMutex;

        const std::string _menuBaseText = "Comware Coder Proxy: ";
        int64_t _codeWindowHandle{};
        std::atomic<bool> _isRunning{true};
        std::atomic<int64_t> _menuHandle{-1}, _menuItemIndex{-1}, _popListWindowHandle{-1};
        std::atomic<types::Time> _debounceRetrieveInfoTime;
        std::unordered_map<uint32_t, uint32_t> _fileHandleMap;

        void _addEditorWindowHandle(uint32_t windowHandle);

        void _threadInitMenuHandle();
    };
}
