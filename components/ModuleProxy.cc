#include <components/ModuleProxy.h>
#include <utils/system.h>

#include <windows.h>

using namespace components;
using namespace std;
using namespace utils;

ModuleProxy::ModuleProxy() : _hModule(LoadLibrary(system::getSystemPath(PROXY_MODULE_NAME).c_str()), FreeLibrary) {
    if (!this->_hModule) {
        throw runtime_error("Failed to load 'msimg32.dll'.");
    }
}
