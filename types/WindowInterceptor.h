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
        void addHandler(UserAction userAction, T *const other, void(T::* const memberFunction)(unsigned int)) {
            _handlers[userAction] = std::bind_front(memberFunction, other);
        }

        [[maybe_unused]] void addHandler(UserAction userAction, CallBackFunction function);

        bool sendAcceptCompletion();

        bool sendCancelCompletion();

        bool sendInsertCompletion();

        bool sendRetrieveInfo();

        bool sendSave();

        bool sendUndo();

    private:
        helpers::KeyHelper _keyHelper;
        std::atomic<bool> _isRunning = true;
        std::atomic<int64_t> _codeWindow = -1, _popListWindow = -1;
        std::shared_ptr<void> _windowHook = nullptr;
        std::string _popListWindowName;
        std::unordered_map<UserAction, CallBackFunction> _handlers;

        void _handleKeycode(Keycode keycode) noexcept;

        void _processWindowMessage(long lParam);

        static long __stdcall _windowProcedureHook(int nCode, unsigned int wParam, long lParam);
    };
}