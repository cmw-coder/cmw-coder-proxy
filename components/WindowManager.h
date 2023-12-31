#pragma once

#include <any>

#include <singleton_dclp.hpp>

#include <helpers/KeyHelper.h>

namespace components {
    class WindowManager : public SingletonDclp<WindowManager> {
    public:
        WindowManager();

        ~WindowManager() override;

        bool checkNeedHideWhenLostFocus(int64_t windowHandle);

        bool checkNeedShowWhenGainFocus(int64_t windowHandle);

        std::tuple<int64_t, int64_t> getClientPosition() const;

        bool hasPopListWindow() const;

        bool hasValidCodeWindow() const;

        void interactionPaste(const std::any& = {});

        void requestRetrieveInfo();

        void sendLeftThenRight() const;

        bool sendSave();

        bool sendUndo();

    private:
        helpers::KeyHelper _keyHelper;
        std::atomic<bool> _isRunning{true}, _needRetrieveInfo{false};
        std::atomic<int64_t> _codeWindowHandle{-1}, _popListWindowHandle{-1};
        std::atomic<types::Time> _debounceRetrieveInfoTime;

        void _cancelRetrieveInfo();

        void _threadDebounceRetrieveInfo();
    };
}
