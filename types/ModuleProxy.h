#pragma once

#include <string>

#include <windows.h>

typedef void (__cdecl *RemoteFunc)(void);

namespace types {
    class ModuleProxy {
    public:
        explicit ModuleProxy(std::string &&moduleName);

        bool load();

        bool free();

        bool hookWindowProc();

        [[maybe_unused]] bool hookKeyboardProc();

        bool unhookWindowProc();

        [[maybe_unused]] bool unhookKeyboardProc();

        RemoteFunc getRemoteFunction(const std::string &procName);

    private:
        std::atomic<bool> isLoaded = false;
        std::string _moduleName;
        HMODULE _hModule = nullptr;
        HHOOK _windowHook = nullptr, _keyboardHook = nullptr;
    };
}

