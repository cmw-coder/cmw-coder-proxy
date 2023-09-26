#include <format>

#include <types/Configurator.h>
#include <types/CursorMonitor.h>
#include <types/ModuleProxy.h>
#include <types/RegistryMonitor.h>
#include <types/WindowInterceptor.h>
#include <utils/logger.h>
#include <utils/system.h>

#include <windows.h>

using namespace std;
using namespace types;
using namespace utils;

namespace {
    void initialize() {
        logger::log("Comware Coder Proxy is initializing...");

        ModuleProxy::Construct();
        Configurator::Construct();
        CursorMonitor::Construct();
        RegistryMonitor::Construct();
        WindowInterceptor::Construct();
    }

    void finalize() {
        logger::log("Comware Coder Proxy is finalizing...");

        WindowInterceptor::Destruct();
        RegistryMonitor::Destruct();
        CursorMonitor::Destruct();
        Configurator::Destruct();
        ModuleProxy::Destruct();
    }
}

#ifdef __cplusplus
extern "C" {
#endif

[[maybe_unused]] BOOL __stdcall DllMain(HMODULE hModule, DWORD dwReason, [[maybe_unused]] PVOID pvReserved) {
    switch (dwReason) {
        case DLL_PROCESS_ATTACH: {
            DisableThreadLibraryCalls(hModule);

            initialize();

            CursorMonitor::GetInstance()->addHandler(
                    UserAction::DeleteBackward,
                    RegistryMonitor::GetInstance(),
                    &RegistryMonitor::cancelByDeleteBackward
            );
            CursorMonitor::GetInstance()->addHandler(
                    UserAction::Navigate,
                    RegistryMonitor::GetInstance(),
                    &RegistryMonitor::cancelByCursorNavigate
            );

            WindowInterceptor::GetInstance()->addHandler(
                    UserAction::Accept,
                    RegistryMonitor::GetInstance(),
                    &RegistryMonitor::acceptByTab
            );
            WindowInterceptor::GetInstance()->addHandler(
                    UserAction::ModifyLine,
                    RegistryMonitor::GetInstance(),
                    &RegistryMonitor::cancelByModifyLine
            );
            WindowInterceptor::GetInstance()->addHandler(
                    UserAction::Navigate,
                    RegistryMonitor::GetInstance(),
                    &RegistryMonitor::cancelByKeycodeNavigate
            );
            WindowInterceptor::GetInstance()->addHandler(
                    UserAction::Normal,
                    [](unsigned int) {
                        WindowInterceptor::GetInstance()->sendFunctionKey(VK_F11);
                        logger::log("Retrieve editor info.");
                    }
            );

            const auto mainThreadId = system::getMainThreadId();
            logger::log(std::format(
                    "MinorVersion: {}, PID: {}, currentTID: {}, mainTID: {}, mainModuleName: {}",
                    get<2>(system::getVersion()),
                    GetCurrentProcessId(),
                    GetCurrentThreadId(),
                    mainThreadId,
                    system::getModuleFileName(reinterpret_cast<uint64_t>(GetModuleHandle(nullptr)))
            ));

            logger::log("Comware Coder Proxy is ready");
            break;
        }
        case DLL_PROCESS_DETACH: {
            finalize();
            logger::log("Comware Coder Proxy is unloaded");
            break;
        }
        default: {
            break;
        }
    }
    return TRUE;
}

void DllInitialize_Impl() {
    const auto func = ModuleProxy::GetInstance()->getRemoteFunction("DllInitialize");
    if (!func) {
        logger::log("FATAL: Failed to get address of 'DllInitialize'.");
        return;
    }
    func();
}

void vSetDdrawflag_Impl() {
    const auto remoteFunction = ModuleProxy::GetInstance()->getRemoteFunction("vSetDdrawflag");
    if (!remoteFunction) {
        logger::log("FATAL: Failed to get address of 'vSetDdrawflag'.");
        return;
    }
    remoteFunction();
}

void AlphaBlend_Impl() {
    const auto remoteFunction = ModuleProxy::GetInstance()->getRemoteFunction("AlphaBlend");
    if (!remoteFunction) {
        logger::log("FATAL: Failed to get address of 'AlphaBlend'.");
        return;
    }
    remoteFunction();
}

void GradientFill_Impl() {
    const auto remoteFunction = ModuleProxy::GetInstance()->getRemoteFunction("GradientFill");
    if (!remoteFunction) {
        logger::log("FATAL: Failed to get address of 'GradientFill'.");
        return;
    }
    remoteFunction();
}

void TransparentBlt_Impl() {
    const auto remoteFunction = ModuleProxy::GetInstance()->getRemoteFunction("TransparentBlt");
    if (!remoteFunction) {
        logger::log("FATAL: Failed to get address of 'TransparentBlt'.");
        return;
    }
    remoteFunction();
}

#ifdef __cplusplus
}
#endif
