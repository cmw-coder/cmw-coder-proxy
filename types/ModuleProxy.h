#pragma once

#include <functional>
#include <string>

#include <singleton_dclp.hpp>

namespace types {
    class ModuleProxy : public SingletonDclp<ModuleProxy> {
    public:
        ModuleProxy();

        std::function<int()> getRemoteFunction(const std::string &procName);

    private:
        std::string _moduleName;
        std::shared_ptr<void> _hModule = nullptr;
    };
}

