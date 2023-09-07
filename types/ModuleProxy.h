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

        RemoteFunc getRemoteFunction(const std::string &procName);

    private:
        std::string moduleName;
        HMODULE m_hModule = nullptr;
    };

}

