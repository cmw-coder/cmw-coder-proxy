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

        bool hookKeyboardProc();

        bool unhookWindowProc();

        bool unhookKeyboardProc();

        RemoteFunc getRemoteFunction(const std::string &procName);

    private:
        std::string _moduleName;
        HMODULE _hModule = nullptr;
        HHOOK _windowHook = nullptr, _keyboardHook = nullptr;
    };
}

