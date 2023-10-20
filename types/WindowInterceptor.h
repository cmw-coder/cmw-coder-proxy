#pragma once

#include <functional>
#include <memory>
#include <unordered_map>

#include <singleton_dclp.hpp>

#include <types/Key.h>
#include <types/SiVersion.h>
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

        void addHandler(UserAction userAction, CallBackFunction function);

        bool sendAcceptCompletion();

        bool sendCancelCompletion();

        bool sendInsertCompletion();

        bool sendRetrieveInfo();

        bool sendSave();

        bool sendUndo();

    private:
        std::shared_ptr<void> _windowHook = nullptr;
        std::atomic<void *> _codeWindow = nullptr;
        std::unordered_map<UserAction, CallBackFunction> _handlers;

        static long __stdcall _windowProcedureHook(int nCode, unsigned int wParam, long lParam);

        void _processWindowMessage(long lParam);

        void _handleKeycode(unsigned int keycode) noexcept;
    };
}