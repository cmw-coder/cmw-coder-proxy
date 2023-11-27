#pragma once

#include <singleton_dclp.hpp>

#include <helpers/KeyHelper.h>

namespace components {
    class WindowManager : public SingletonDclp<WindowManager> {
    public:
        WindowManager();

        ~WindowManager() override;

        bool checkLostFocus(int64_t windowHandle);

        bool checkGainFocus(int64_t windowHandle);

        void requestRetrieveInfo();

        bool sendAcceptCompletion();

        bool sendCancelCompletion();

        bool sendDoubleInsert() const;

        bool sendInsertCompletion();

    private:
        helpers::KeyHelper _keyHelper;
        std::atomic<bool> _isRunning = true, _needRetrieveInfo = false;
        std::atomic<int64_t> _codeWindowHandle = -1, _popListWindowHandle = -1;
        std::atomic<std::chrono::time_point<std::chrono::high_resolution_clock>> _debounceTime;

        void _cancelRetrieveInfo();

        void _threadDebounceRetrieveInfo();
    };
}
