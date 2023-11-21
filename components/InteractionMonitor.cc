#include <components/InteractionMonitor.h>
#include <types/SiVersion.h>
#include <utils/window.h>

#include <windows.h>

using namespace components;
// using namespace helpers;
// using namespace magic_enum;
using namespace std;
using namespace types;
using namespace utils;

namespace {
    const unordered_map<SiVersion::Major, unordered_map<SiVersion::Minor, tuple<uint64_t, uint64_t>>> addressMap = {
        {
            SiVersion::Major::V35, {
                {SiVersion::Minor::V0076, {0x1CBEFC, 0x1CBF00}},
                {SiVersion::Minor::V0086, {0x1BE0CC, 0x1CD3E0}}
            }
        },
        {
            SiVersion::Major::V40, {
                {SiVersion::Minor::V0084, {0x268A60, 0x268A64}},
                {SiVersion::Minor::V0086, {0x26D938, 0x26D93C}},
                {SiVersion::Minor::V0088, {0x26EA08, 0x26EA0C}},
                {SiVersion::Minor::V0089, {0x26EAC8, 0x26EACC}},
                {SiVersion::Minor::V0096, {0x278D30, 0x278D34}},
                {SiVersion::Minor::V0116, {0x27E468, 0x27E46C}},
                {SiVersion::Minor::V0118, {0x27F490, 0x27F494}},
                {SiVersion::Minor::V0120, {0x2807F8, 0x2807FC}},
                {SiVersion::Minor::V0124, {0x284DF0, 0x284DF4}},
                {SiVersion::Minor::V0130, {0x289F9C, 0x289FA0}},
                {SiVersion::Minor::V0132, {0x28B2FC, 0x28B300}}
            }
        },
    };
}

InteractionMonitor::InteractionMonitor() : _processHandle(GetCurrentProcess(), CloseHandle),
                                           _windowHookHandle(
                                               SetWindowsHookEx(
                                                   WH_CALLWNDPROC,
                                                   _windowProcedureHook,
                                                   nullptr,
                                                   GetCurrentThreadId()
                                               ),
                                               UnhookWindowsHookEx
                                           ) {
    if (!_processHandle) {
        throw runtime_error("Failed to get Source Insight's process handle");
    }
    if (!_windowHookHandle) {
        throw runtime_error("Failed to set window hook.");
    }
}

void InteractionMonitor::_processWindowMessage(const long lParam) {
    const auto windowProcData = reinterpret_cast<PCWPSTRUCT>(lParam);
    const auto currentWindow = reinterpret_cast<int64_t>(windowProcData->hwnd);
    if (const auto currentWindowClass = window::getWindowClassName(currentWindow); currentWindowClass == "si_Sw") {
        switch (windowProcData->message) {
            case WM_KILLFOCUS: {
                if (const auto targetWindowClass = window::getWindowClassName(windowProcData->wParam);
                    _codeWindowHandle >= 0 && targetWindowClass != "si_Poplist") {
                    // logger::log(format(
                    //         "Coding window '{}' lost focus. (0x{:08X} '{}') to (0x{:08X} '{}')",
                    //         window::getWindowText(currentWindow),
                    //         currentWindow,
                    //         currentWindowClass,
                    //         static_cast<uint64_t>(windowProcData->wParam),
                    //         targetWindowClass
                    // ));
                    _handlers.at(Interaction::LostFocus)(windowProcData->wParam);
                    _codeWindowHandle.store(-1);
                }
                else if (targetWindowClass == "si_Poplist") {
                    _popListWindowHandle.store(windowProcData->wParam);
                }
                break;
            }
            case WM_MOUSEACTIVATE: {
                // CursorMonitor::GetInstance()->setAction(UserAction::Navigate);
                break;
            }
            case WM_SETFOCUS: {
                // const auto targetWindowClass = window::getWindowClassName(windowProcData->wParam);
                // logger::log(format(
                //         "Coding window '{}' set focus. (0x{:08X} '{}') from (0x{:08X} '{}')",
                //         window::getWindowText(currentWindow),
                //         currentWindow,
                //         currentWindowClass,
                //         static_cast<uint64_t>(windowProcData->wParam),
                //         targetWindowClass
                // ));
                if (_codeWindowHandle < 0) {
                    _codeWindowHandle.store(currentWindow);
                }
                if (_popListWindowHandle > 0) {
                    _popListWindowHandle.store(-1);
                    // sendCancelCompletion();
                }
                break;
            }
            case UM_KEYCODE: {
                // _handleKeycode(windowProcData->wParam);
                break;
            }
            default: {
                break;
            }
        }
    }
}

long InteractionMonitor::_windowProcedureHook(const int nCode, const unsigned int wParam, const long lParam) {
    GetInstance()->_processWindowMessage(lParam);
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}
