#pragma once

#include <functional>
#include <memory>
#include <unordered_map>

#include <singleton_dclp.hpp>

#include <types/CursorMonitor.h>
#include <types/SiVersion.h>

namespace types {
    class WindowInterceptor : public SingletonDclp<WindowInterceptor> {
    public:
        enum class ActionType {
            Accept,
            DeleteBackward,
            DeleteForward,
            ModifyLine,
            Navigate,
            Normal,
        };

        using CallBackFunction = std::function<void(unsigned int)>;

        WindowInterceptor();

        template<class T>
        void addHandler(ActionType keyType, T *const other, CallBackFunction memberFunction) {
            _handlers[keyType] = std::bind_front(memberFunction, other);
        }

        void addHandler(ActionType keyType, CallBackFunction function);

        bool sendFunctionKey(int key);

    private:
        CursorMonitor _cursorMonitor;
        std::shared_ptr<void> _windowHook = nullptr;
        std::atomic<SiVersion> _siVersion = SiVersion::Unknown;
        std::atomic<void *> _codeWindow = nullptr;
        std::unordered_map<ActionType, CallBackFunction> _handlers;


        static long __stdcall _windowProcedureHook(int nCode, unsigned int wParam, long lParam);

        void _processWindowMessage(long lParam);

        void _handleKeycode(unsigned int keycode) noexcept;
    };
}