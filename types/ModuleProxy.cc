#include <vector>

#include <types/ModuleProxy.h>
#include <utils/logger.h>
#include <utils/system.h>

using namespace std;
using namespace types;
using namespace utils;

namespace {
    const auto UM_KEYCODE = WM_USER + 0x03E9;
}

ModuleProxy::ModuleProxy(std::string &&moduleName) : _moduleName(std::move(moduleName)) {}

bool ModuleProxy::load() {
    this->_hModule = LoadLibrary(system::getSystemPath(PROXY_MODULE_NAME).c_str());
    return this->_hModule;
}

bool ModuleProxy::free() {
    if (!this->_hModule) {
        return false;
    }
    return !FreeLibrary(this->_hModule);
}

RemoteFunc ModuleProxy::getRemoteFunction(const std::string &procName) {
    if (!this->_hModule) {
        return nullptr;
    }
    return reinterpret_cast<RemoteFunc>(GetProcAddress(this->_hModule, procName.c_str()));
}