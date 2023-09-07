#include <types/ModuleProxy.h>
#include <utility>

using namespace std;
using namespace types;

string getSystemDirectory() {
    string systemDirectory;
    systemDirectory.resize(MAX_PATH);
    GetSystemDirectory(systemDirectory.data(), MAX_PATH);
    systemDirectory.resize(strlen(systemDirectory.data()));
    return systemDirectory;
}

ModuleProxy::ModuleProxy(std::string &&moduleName) {
    this->moduleName = std::move(moduleName);
}

bool ModuleProxy::load() {
    const auto modulePath = getSystemDirectory() + R"(\)" + moduleName;
    this->m_hModule = LoadLibrary(modulePath.data());
    return this->m_hModule != nullptr;
}

bool ModuleProxy::free() {
    if (!this->m_hModule) {
        return false;
    }
    return FreeLibrary(this->m_hModule) != FALSE;
}

RemoteFunc ModuleProxy::getRemoteFunction(const std::string &procName) {
    if (!this->m_hModule) {
        return nullptr;
    }
    return reinterpret_cast<RemoteFunc>(GetProcAddress(this->m_hModule, procName.c_str()));
}
