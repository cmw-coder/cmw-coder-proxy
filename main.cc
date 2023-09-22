#include <format>

#include <types/ModuleProxy.h>
#include <types/WindowInterceptor.h>
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

[[maybe_unused]] BOOL __stdcall DllMain(HMODULE hModule, DWORD dwReason, [[maybe_unused]] PVOID pvReserved) {
    switch (dwReason) {
        case DLL_PROCESS_ATTACH: {
            DisableThreadLibraryCalls(hModule);
            const auto result = moduleProxy.load();
            if (!result) {
                logger::log("Failed to load 'msimg32.dll'.");
                return FALSE;
            }
            logger::log("Successfully hooked 'msimg32.dll'.");

            CursorMonitor::Construct();
            WindowInterceptor::Construct();

            CursorMonitor::GetInstance()->addHandler(
                    UserAction::DeleteBackward,
                    [](CursorMonitor::CursorPosition oldPosition, CursorMonitor::CursorPosition newPosition) {
                        if (oldPosition.line == newPosition.line) {
                            system::setRegValue("Software/Source Dynamics/Source Insight/3.0", "cancelType", 2);
                            WindowInterceptor::GetInstance()->sendFunctionKey(VK_F9);
                            logger::log(format("Deleted backward"));
                        } else {
                            system::setRegValue("Software/Source Dynamics/Source Insight/3.0", "cancelType", 3);
                            WindowInterceptor::GetInstance()->sendFunctionKey(VK_F9);
                            logger::log(format("Modified line (Backspace)"));
                        }
                    }
            );
            CursorMonitor::GetInstance()->addHandler(
                    UserAction::Navigate,
                    [](CursorMonitor::CursorPosition oldPosition, CursorMonitor::CursorPosition newPosition) {
                        system::setRegValue("Software/Source Dynamics/Source Insight/3.0", "cancelType", 1);
                        WindowInterceptor::GetInstance()->sendFunctionKey(VK_F9);
                        logger::log(format("Navigated (Scanned)"));
                    }
            );

            WindowInterceptor::GetInstance()->addHandler(UserAction::Accept, [](unsigned int keycode) {
                WindowInterceptor::GetInstance()->sendFunctionKey(VK_F10);
                logger::log(format("Accepted completion, keycode 0x{:08X}", keycode));
            });
            WindowInterceptor::GetInstance()->addHandler(UserAction::ModifyLine, [](unsigned int keycode) {
                system::setRegValue("Software/Source Dynamics/Source Insight/3.0", "cancelType", 3);
                WindowInterceptor::GetInstance()->sendFunctionKey(VK_F9);
                logger::log(format("Modified line (Enter)"));
            });
            WindowInterceptor::GetInstance()->addHandler(UserAction::Navigate, [](unsigned int keycode) {
                system::setRegValue("Software/Source Dynamics/Source Insight/3.0", "cancelType", 1);
                WindowInterceptor::GetInstance()->sendFunctionKey(VK_F9);
                logger::log(format("Navigated, keycode 0x{:08X}", keycode));
            });
            WindowInterceptor::GetInstance()->addHandler(UserAction::Normal, [](unsigned int keycode) {
                WindowInterceptor::GetInstance()->sendFunctionKey(VK_F11);
                logger::log(format("Normal, keycode 0x{:08X}", keycode));
                thread([] {
                    // TODO: implement completion generation
                    this_thread::sleep_for(chrono::milliseconds(1000));
                    system::setRegValue(
                            "Software/Source Dynamics/Source Insight/3.0",
                            "completionGenerated",
                            "Lorem ipsum dolor sit amet"
                    );
                    WindowInterceptor::GetInstance()->sendFunctionKey(VK_F12);
                    logger::log("Generated completion");
                }).detach();
            });

            const auto mainThreadId = system::getMainThreadId();
            logger::log(std::format(
                    "ProcessId: {}, currentThreadId: {}, mainThreadId: {}, mainModuleName: {}",
                    GetCurrentProcessId(),
                    GetCurrentThreadId(),
                    mainThreadId,
                    system::getModuleFileName(reinterpret_cast<uint64_t>(GetModuleHandle(nullptr)))
            ));

            break;
        }
        case DLL_PROCESS_DETACH: {
            moduleProxy.free();
            CursorMonitor::Destruct();
            WindowInterceptor::Destruct();
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