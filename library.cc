#include <types/ModuleProxy.h>
#include <utils/logger.h>

#include <tlhelp32.h>

using namespace std;
using namespace types;
using namespace utils;

static auto moduleProxy = ModuleProxy("msimg32");

#define WM_UAHDRAWMENUITEM              0x0092
#define UM_GETSELCOUNT                  (WM_USER + 1000)
#define UM_GETUSERSELA                  (WM_USER + 1001)
#define UM_GETUSERSELW                  (WM_USER + 1002)
#define UM_GETGROUPSELA                 (WM_USER + 1003)
#define UM_GETGROUPSELW                 (WM_USER + 1004)
#define UM_GETCURFOCUSA                 (WM_USER + 1005)
#define UM_GETCURFOCUSW                 (WM_USER + 1006)
#define UM_GETOPTIONS                   (WM_USER + 1007)
#define UM_GETOPTIONS2                  (WM_USER + 1008)

DWORD GetMainThreadId() {
    const shared_ptr<void> hThreadSnapshot(
            CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0), CloseHandle);
    if (hThreadSnapshot.get() == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("GetMainThreadId failed");
    }
    THREADENTRY32 tEntry;
    tEntry.dwSize = sizeof(THREADENTRY32);
    DWORD result = 0;
    DWORD currentPID = GetCurrentProcessId();
    for (BOOL success = Thread32First(hThreadSnapshot.get(), &tEntry);
         !result && success && GetLastError() != ERROR_NO_MORE_FILES;
         success = Thread32Next(hThreadSnapshot.get(), &tEntry)) {
        logger::log(format(
                "tEntry.th32OwnerProcessID: {}, currentPID: {}",
                tEntry.th32OwnerProcessID,
                currentPID
        ));
        if (tEntry.th32OwnerProcessID == currentPID) {
            result = tEntry.th32ThreadID;
        }
    }
    return result;
}

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
                return FALSE;
            }
            if (SetWindowsHookEx(WH_CALLWNDPROC, reinterpret_cast<HOOKPROC>((void *) [](INT nCode, WPARAM wParam, LPARAM lParam) -> LRESULT {
                const auto msg = reinterpret_cast<CWPSTRUCT *>(lParam);
                switch (msg->message) {
                    case UM_GETCURFOCUSA:
                    case UM_GETCURFOCUSW:
                    case WM_CTLCOLOREDIT:
                    case WM_DRAWITEM:
                    case WM_GETTEXT:
                    case WM_NCHITTEST:
                    case WM_SETCURSOR:
                    case WM_PSD_MINMARGINRECT:
                    case WM_UAHDRAWMENUITEM: {
                        break;
                    }
                    case WM_COMMAND: {
                        logger::log(format(
                                "WH_CALLWNDPROC: isCurr: {}, hwnd: 0x{:08X}, message: WM_COMMAND, high: 0x{:04X}, low: 0x{:04X}, lParam: 0x{:08X}",
                                wParam != 0,
                                reinterpret_cast<uint64_t>(msg->hwnd),
                                HIWORD(msg->wParam),
                                LOWORD(msg->wParam),
                                msg->lParam
                        ));
                        break;
                    }
                    default: {
                        logger::log(format(
                                "WH_CALLWNDPROC: isCurr: {}, hwnd: 0x{:08X}, message: 0x{:04X}, wParam: {}, lParam: {}",
                                wParam != 0,
                                reinterpret_cast<uint64_t>(msg->hwnd),
                                msg->message,
                                msg->wParam,
                                msg->lParam
                        ));
                        break;
                    }
                }
                return CallNextHookEx(nullptr, nCode, wParam, lParam);
            }), nullptr, GetCurrentThreadId()) == nullptr) {
                logger::log(format("Hook Error: {}", GetLastError()));
                return FALSE;
            }
            uint32_t pid = GetCurrentProcessId();
            logger::log("Successfully loaded 'msimg32.dll'.");
            logger::log(format("Current PID: {}", pid));
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