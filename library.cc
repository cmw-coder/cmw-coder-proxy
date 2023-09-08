#include <types/ModuleProxy.h>
#include <utils/logger.h>

using namespace std;
using namespace types;
using namespace utils;

static auto moduleProxy = ModuleProxy("msimg32");

#ifdef __cplusplus
extern "C" {
#endif

[[maybe_unused]] BOOL WINAPI DllMain(HMODULE hModule, DWORD dwReason, [[maybe_unused]] PVOID pvReserved) {
    switch (dwReason) {
        case DLL_PROCESS_ATTACH: {
            DisableThreadLibraryCalls(hModule);
            const auto result = moduleProxy.load();
            if (!result) {
                logger::log("Failed to load 'msimg32.dll'.");
                if (SetWindowsHookEx(WH_CALLWNDPROC, reinterpret_cast<HOOKPROC>((void *) [](INT nCode, WPARAM wParam, LPARAM lParam) -> LRESULT {
                    logger::log(std::format("WH_GETMESSAGE: nCode: {}, wParam: {}, lParam: {}", nCode, wParam, lParam));
                    return CallNextHookEx(nullptr, nCode, wParam, lParam);
                }), nullptr, 0) == nullptr) {
                    logger::log(std::format("Hook Error: {}", GetLastError()));
                }
                return FALSE;
            }
            logger::log("Successfully loaded 'msimg32.dll'.");
            break;
        }
        case DLL_PROCESS_DETACH: {
            moduleProxy.free();
            break;
        }
        default: {
            break;
        }
    }
    return TRUE;
}

[[maybe_unused]] void __cdecl DllInitialize() {
    const auto func = moduleProxy.getRemoteFunction("DllInitialize");
    if (!func) {
        logger::log("Failed to get address of 'DllInitialize'.");
        return;
    }
    func();
}

[[maybe_unused]] void __cdecl vSetDdrawflag() {
    const auto remoteFunction = moduleProxy.getRemoteFunction("vSetDdrawflag");
    if (!remoteFunction) {
        logger::log("Failed to get address of 'vSetDdrawflag'.");
        return;
    }
    remoteFunction();
}

//[[maybe_unused]] void __cdecl AlphaBlend(void) {
//    auto func = (RemoteFunc) GetAddress("AlphaBlend");
//    func();
//}

//[[maybe_unused]] void __cdecl GradientFill() {
//    auto func = (RemoteFunc) GetAddress("GradientFill");
//    func();
//}

//[[maybe_unused]] void __cdecl TransparentBlt() {
//    auto func = (RemoteFunc) GetAddress("TransparentBlt");
//    func();
//}

#ifdef __cplusplus
}
#endif