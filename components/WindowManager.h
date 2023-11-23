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

        bool sendAcceptCompletion();

        bool sendCancelCompletion();

        bool sendDoubleInsert() const;

        bool sendInsertCompletion();

    private:
        helpers::KeyHelper _keyHelper;
        std::atomic<bool> _isRunning = true, _needRetrieveInfo = false;
        std::atomic<int64_t> _codeWindowHandle = -1, _popListWindowHandle = -1;

        void _cancelRetrieveInfo();
    };
}
