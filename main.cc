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

typedef BOOL (WINAPI *DllInitializeFunc)(
        HINSTANCE hinstDLL,
        DWORD fdwReason,
        LPVOID lpReserved
);

BOOL DllInitialize_Impl(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    const auto remoteFunction = ModuleProxy::GetInstance()->getRemoteFunction<DllInitializeFunc>("DllInitialize");
    if (!remoteFunction) {
        logger::log("FATAL: Failed to get address of 'DllInitialize'.");
        return false;
    }
    return remoteFunction(hinstDLL, fdwReason, lpReserved);
}

typedef void (WINAPI *vSetDdrawflagFunc)();

void vSetDdrawflag_Impl() {
    const auto remoteFunction = ModuleProxy::GetInstance()->getRemoteFunction<vSetDdrawflagFunc>("vSetDdrawflag");
    if (!remoteFunction) {
        logger::log("FATAL: Failed to get address of 'vSetDdrawflag'.");
        return;
    }
    remoteFunction();
}

typedef BOOL (WINAPI *AlphaBlendFunc)(
        HDC hdcDest,
        int xoriginDest,
        int yoriginDest,
        int wDest,
        int hDest,
        HDC hdcSrc,
        int xoriginSrc,
        int yoriginSrc,
        int wSrc,
        int hSrc,
        BLENDFUNCTION ftn
);

BOOL AlphaBlend_Impl(
        HDC hdcDest,
        int xoriginDest,
        int yoriginDest,
        int wDest,
        int hDest,
        HDC hdcSrc,
        int xoriginSrc,
        int yoriginSrc,
        int wSrc,
        int hSrc,
        BLENDFUNCTION ftn
) {
    const auto remoteFunction = ModuleProxy::GetInstance()->getRemoteFunction<AlphaBlendFunc>("AlphaBlend");
    if (!remoteFunction) {
        logger::log("FATAL: Failed to get address of 'AlphaBlend'.");
        return false;
    }
    return remoteFunction(
            hdcDest,
            xoriginDest,
            yoriginDest,
            wDest,
            hDest,
            hdcSrc,
            xoriginSrc,
            yoriginSrc,
            wSrc,
            hSrc,
            ftn
    );
}

typedef BOOL (WINAPI *GradientFillFunc)(
        HDC hdc,
        PTRIVERTEX pVertex,
        ULONG nVertex,
        PVOID pMesh,
        ULONG nMesh,
        ULONG ulMode
);

BOOL GradientFill_Impl(
        HDC hdc,
        PTRIVERTEX pVertex,
        ULONG nVertex,
        PVOID pMesh,
        ULONG nMesh,
        ULONG ulMode
) {
    const auto remoteFunction = ModuleProxy::GetInstance()->getRemoteFunction<GradientFillFunc>("GradientFill");
    if (!remoteFunction) {
        logger::log("FATAL: Failed to get address of 'GradientFill'.");
        return false;
    }
    return remoteFunction(hdc, pVertex, nVertex, pMesh, nMesh, ulMode);
}

typedef BOOL (WINAPI *TransparentBltFunc)(
        HDC hdcDest,
        int xoriginDest,
        int yoriginDest,
        int wDest,
        int hDest,
        HDC hdcSrc,
        int xoriginSrc,
        int yoriginSrc,
        int wSrc,
        int hSrc,
        UINT crTransparent
);

BOOL TransparentBlt_Impl(
        HDC hdcDest,
        int xoriginDest,
        int yoriginDest,
        int wDest,
        int hDest,
        HDC hdcSrc,
        int xoriginSrc,
        int yoriginSrc,
        int wSrc,
        int hSrc,
        UINT crTransparent
) {
    const auto remoteFunction = ModuleProxy::GetInstance()->getRemoteFunction<TransparentBltFunc>("TransparentBlt");
    if (!remoteFunction) {
        logger::log("FATAL: Failed to get address of 'TransparentBlt'.");
        return false;
    }
    return remoteFunction(
            hdcDest,
            xoriginDest,
            yoriginDest,
            wDest,
            hDest,
            hdcSrc,
            xoriginSrc,
            yoriginSrc,
            wSrc,
            hSrc,
            crTransparent
    );
}

#ifdef __cplusplus
}
#endif
