#include <format>

#include <types/ModuleProxy.h>
#include <utils/logger.h>
#include <utils/system.h>
#include <utils/window.h>

using namespace std;
using namespace types;
using namespace utils;

namespace {
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
}

ModuleProxy::ModuleProxy(std::string &&moduleName) {
    this->_moduleName = std::move(moduleName);
}

bool ModuleProxy::load() {
    const auto modulePath = system::getSystemDirectory() + R"(\)" + _moduleName;
    this->_hModule = LoadLibrary(modulePath.data());
    return this->_hModule != nullptr;
}

bool ModuleProxy::free() {
    if (!this->_hModule) {
        return false;
    }
    return FreeLibrary(this->_hModule) != FALSE;
}

RemoteFunc ModuleProxy::getRemoteFunction(const std::string &procName) {
    if (!this->_hModule) {
        return nullptr;
    }
    return reinterpret_cast<RemoteFunc>(GetProcAddress(this->_hModule, procName.c_str()));
}

bool ModuleProxy::hookWindowProc() {
    this->_windowHook = SetWindowsHookEx(WH_CALLWNDPROC, reinterpret_cast<HOOKPROC>((void *) [](INT nCode, WPARAM wParam, LPARAM lParam) -> LRESULT {
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
                // Ignore these messages
                break;
            }
            case WM_SETFOCUS: {
                logger::log(format(
                        "[WH_CALLWNDPROC] [WM_SETFOCUS] from: '{}'(0x{:08X} '{}'), to: '{}'(0x{:08X} '{}')",
                        window::getWindowText(reinterpret_cast<HWND>(msg->wParam)),
                        msg->wParam,
                        window::getWindowClassName(reinterpret_cast<HWND>(msg->wParam)),
                        window::getWindowText(msg->hwnd),
                        reinterpret_cast<uint64_t>(msg->hwnd),
                        window::getWindowClassName(msg->hwnd)
                ));
                break;
            }
//            case WM_COMMAND: {
//                if (HIWORD(msg->wParam) != 0x0300 && HIWORD(msg->wParam) != 0x0400) {
//                    logger::log(format(
//                            "(WH_CALLWNDPROC) isCurr: {}, hwnd: 0x{:08X}, message: WM_COMMAND, high: 0x{:04X}, low: 0x{:04X}, lParam: 0x{:08X}",
//                            wParam != 0,
//                            reinterpret_cast<uint64_t>(msg->hwnd),
//                            HIWORD(msg->wParam),
//                            LOWORD(msg->wParam),
//                            msg->lParam
//                    ));
//                }
//                break;
//            }
            default: {
//              logger::log(format(
//                      "WH_CALLWNDPROC: isCurr: {}, hwnd: 0x{:08X}, message: 0x{:04X}, wParam: {}, lParam: {}",
//                      wParam != 0,
//                      reinterpret_cast<uint64_t>(msg->hwnd),
//                      msg->message,
//                      msg->wParam,
//                      msg->lParam
//              ));
                break;
            }
        }
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }), nullptr, GetCurrentThreadId());

    return this->_windowHook != nullptr;
}

bool ModuleProxy::hookKeyboardProc() {
    this->_keyboardHook = SetWindowsHookEx(WH_KEYBOARD, reinterpret_cast<HOOKPROC>((void *) [](INT nCode, WPARAM wParam, LPARAM lParam) -> LRESULT {
        if (nCode >= 0) {
            WORD vkCode = LOWORD(wParam);
            WORD keyFlags = HIWORD(lParam);
            WORD repeatCount = LOWORD(lParam);  // repeat count, > 0 if several keydown messages was combined into one message
            WORD scanCode = LOBYTE(keyFlags);

            // extended-key flag, 1 if scancode has 0xE0 prefix
            if ((keyFlags & KF_EXTENDED) == KF_EXTENDED) {
                scanCode = MAKEWORD(scanCode, 0xE0);
            }
            bool wasKeyDown = (keyFlags & KF_REPEAT) == KF_REPEAT;  // previous key-state flag, 1 on auto repeat
            bool isKeyReleased = (keyFlags & KF_UP) == KF_UP;   // transition-state flag, 1 on keyup

            // if we want to distinguish these keys:
//          switch (vkCode) {
//              case VK_SHIFT:   // converts to VK_LSHIFT or VK_RSHIFT
//              case VK_CONTROL: // converts to VK_LCONTROL or VK_RCONTROL
//              case VK_MENU:    // converts to VK_LMENU or VK_RMENU
//                  vkCode = LOWORD(MapVirtualKeyW(scanCode, MAPVK_VSC_TO_VK_EX));
//                  break;
//          }

            logger::log(format(
                    "WH_KEYBOARD: vkCode: 0x{:04X}, scanCode: 0x{:04X}, wasKeyDown: {}, repeatCount: {}, isKeyReleased: {}",
                    vkCode,
                    scanCode,
                    wasKeyDown,
                    repeatCount,
                    isKeyReleased
            ));
        }
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }), nullptr, GetCurrentThreadId());

    return this->_keyboardHook != nullptr;
}

bool ModuleProxy::unhookWindowProc() {
    return UnhookWindowsHookEx(this->_windowHook);
}

bool ModuleProxy::unhookKeyboardProc() {
    return UnhookWindowsHookEx(this->_keyboardHook);
}
