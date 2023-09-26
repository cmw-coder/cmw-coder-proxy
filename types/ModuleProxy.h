#pragma once

#include <functional>
#include <string>

#include <singleton_dclp.hpp>

#include <windows.h>

namespace types {
    class ModuleProxy : public SingletonDclp<ModuleProxy> {
    public:
        ModuleProxy();

        template<class T>
        T getRemoteFunction(const std::string &procName) {
            return (T) GetProcAddress(
                    reinterpret_cast<HMODULE>(this->_hModule.get()),
                    procName.c_str()
            );
        }

    private:
        std::string _moduleName;
        std::shared_ptr<void> _hModule = nullptr;
    };
}

