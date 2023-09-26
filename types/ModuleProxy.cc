#include <types/ModuleProxy.h>
#include <utils/logger.h>
#include <utils/system.h>

#include <windows.h>

using namespace std;
using namespace types;
using namespace utils;

namespace {
    const auto UM_KEYCODE = WM_USER + 0x03E9;
}

ModuleProxy::ModuleProxy() :
        _hModule(LoadLibrary(system::getSystemPath(PROXY_MODULE_NAME).c_str()), FreeLibrary) {
    if (!this->_hModule) {
        throw runtime_error("Failed to load 'msimg32.dll'.");
    }
}

function<int()> ModuleProxy::getRemoteFunction(const string &procName) {
    return GetProcAddress(
            reinterpret_cast<HMODULE>(this->_hModule.get()),
            procName.c_str()
    );
}
