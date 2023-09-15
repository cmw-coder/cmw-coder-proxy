#include <format>

#include <utils/httplib.h>

#include <types/ModuleProxy.h>
#include <utils/logger.h>
#include <utils/system.h>

using namespace std;
using namespace types;
using namespace utils;

namespace {
    auto moduleProxy = ModuleProxy("msimg32");
}

#ifdef __cplusplus
extern "C" {
#endif

[[maybe_unused]] BOOL WINAPI DllMain(HMODULE hModule, DWORD dwReason, [[maybe_unused]] PVOID pvReserved) {
    switch (dwReason) {
        case DLL_PROCESS_ATTACH: {
            DisableThreadLibraryCalls(hModule);
            const auto result = moduleProxy.load();
            logger::log("Successfully hooked 'msimg32.dll'.");
            const auto mainThreadId = system::getMainThreadId();

            logger::log(std::format(
                    "ProcessId: {}, currentThreadId: {}, mainThreadId: {}, mainModuleName: {}",
                    GetCurrentProcessId(),
                    GetCurrentThreadId(),
                    mainThreadId,
                    system::getModuleFileName(reinterpret_cast<uint64_t>(GetModuleHandle(nullptr)))
            ));

            if (!result) {
                logger::log("Failed to load 'msimg32.dll'.");
                return FALSE;
            }
            if (!moduleProxy.hookWindowProc()) {
                logger::log(format("Hook 'WH_CALLWNDPROC' Error: {}", GetLastError()));
                return FALSE;
            }

            thread([]() {
                httplib::Server server;
                server.Get("/hi", [](const httplib::Request &, httplib::Response &res) {
                    res.set_content("Hello World!", "text/plain");
                });
                if (!server.listen("127.0.0.1", 23333)) {
                    logger::log("Failed to start server.");
                }
            }).detach();

            break;
        }
        case DLL_PROCESS_DETACH: {
            moduleProxy.unhookWindowProc();
            moduleProxy.free();
            logger::log("Successfully unhooked 'msimg32.dll'.");
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