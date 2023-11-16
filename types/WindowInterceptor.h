#pragma once

#include <functional>
#include <memory>
#include <unordered_map>

#include <singleton_dclp.hpp>

#include <helpers/KeyHelper.h>
#include <types/UserAction.h>

namespace types {
    class WindowInterceptor : public SingletonDclp<WindowInterceptor> {
    public:
        using CallBackFunction = std::function<void(unsigned int)>;

        WindowInterceptor();

        template<class T>
        void addHandler(const UserAction userAction, T* const other, void (T::* const memberFunction)(unsigned int)) {
            _handlers[userAction] = std::bind_front(memberFunction, other);
        }

        [[maybe_unused]] void addHandler(UserAction userAction, CallBackFunction function);

        void requestRetrieveInfo();

        bool sendAcceptCompletion();

        bool sendCancelCompletion();

        bool sendInsertCompletion();

        bool sendSave();

        bool sendUndo();

    private:
        helpers::KeyHelper _keyHelper;
        std::atomic<bool> _isRunning = true, _needRetrieveInfo = false;
        std::atomic<int64_t> _codeWindow = -1, _popListWindow = -1;
        std::atomic<std::chrono::time_point<std::chrono::high_resolution_clock>> _debounceTime;
        std::shared_ptr<void> _windowHook = nullptr;
        std::string _popListWindowName;
        std::unordered_map<UserAction, CallBackFunction> _handlers;

        void _cancelRetrieveInfo();

        void _handleKeycode(Keycode keycode) noexcept;

        void _processWindowMessage(long lParam);

        void _threadDebounceRetrieveInfo();

        static long __stdcall _windowProcedureHook(int nCode, unsigned int wParam, long lParam);
    };
}
