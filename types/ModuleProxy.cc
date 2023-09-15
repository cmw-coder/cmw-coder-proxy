#include <format>
#include <vector>

#include <types/ModuleProxy.h>
#include <utils/logger.h>
#include <utils/system.h>
#include <utils/window.h>

using namespace std;
using namespace types;
using namespace utils;

namespace {
    const auto UM_KEYCODE = WM_USER + 0x03E9;
    atomic<HWND> codeWindow = nullptr;
}

ModuleProxy::ModuleProxy(std::string &&moduleName) {
    this->_moduleName = std::move(moduleName);
}

bool ModuleProxy::load() {
    const auto modulePath = system::getSystemDirectory() + R"(\)" + _moduleName;
    this->_hModule = LoadLibrary(modulePath.data());
    this->isLoaded = this->_hModule != nullptr;
    thread([this]() {
        bool needDelay = true;
        while (this->isLoaded.load()) {
            if (!codeWindow.load()) {
                needDelay = true;
                this_thread::sleep_for(chrono::milliseconds(50));
                continue;
            }
            if (needDelay) {
                needDelay = false;
                this_thread::sleep_for(chrono::milliseconds(2000));
                continue;
            }
            window::sendFunctionKey(codeWindow.load(), VK_F12);
            this_thread::sleep_for(chrono::milliseconds(200));
        }
    }).detach();
    return this->isLoaded;
}

bool ModuleProxy::free() {
    if (!this->_hModule) {
        return false;
    }
    this->isLoaded = false;
    return FreeLibrary(this->_hModule) != FALSE;
}

RemoteFunc ModuleProxy::getRemoteFunction(const std::string &procName) {
    if (!this->_hModule) {
        return nullptr;
    }
    return reinterpret_cast<RemoteFunc>(GetProcAddress(this->_hModule, procName.c_str()));
}

bool ModuleProxy::hookWindowProc() {
    this->_windowHook = SetWindowsHookEx(
            WH_CALLWNDPROC,
            reinterpret_cast<HOOKPROC>((void *) [](INT nCode, WPARAM wParam, LPARAM lParam) -> LRESULT {
                const auto windowProcData = reinterpret_cast<PCWPSTRUCT>(lParam);
                switch (windowProcData->message) {
                    case WM_KILLFOCUS: {
                        const auto currentWindow = windowProcData->hwnd;
                        const auto targetWindow = reinterpret_cast<HWND>(windowProcData->wParam);
                        if (window::getWindowClassName(currentWindow) == "si_Sw" &&
                            window::getWindowClassName(currentWindow) != window::getWindowClassName(targetWindow) &&
                            codeWindow.load()) {
                            logger::log(format(
                                    "Coding window '{}' lost focus. (0x{:08X} '{}')",
                                    window::getWindowText(currentWindow),
                                    reinterpret_cast<uint64_t>(currentWindow),
                                    window::getWindowClassName(currentWindow)
                            ));
                            codeWindow.store(nullptr);
                        }
                        break;
                    }
                    case WM_SETFOCUS: {
                        const auto currentWindow = windowProcData->hwnd;
                        const auto targetWindow = reinterpret_cast<HWND>(windowProcData->wParam);
                        if (window::getWindowClassName(currentWindow) == "si_Sw" &&
                            window::getWindowClassName(currentWindow) != window::getWindowClassName(targetWindow) &&
                            !codeWindow.load()) {
                            logger::log(format(
                                    "Coding window '{}' gained focus. (0x{:08X} '{}')",
                                    window::getWindowText(currentWindow),
                                    reinterpret_cast<uint64_t>(currentWindow),
                                    window::getWindowClassName(currentWindow)
                            ));
                            codeWindow.store(currentWindow);
                        }
                        break;
                    }
                    case UM_KEYCODE: {
                        if (windowProcData->wParam != 0x1F7B) {
                            try {
                                system::setRegValue32(
                                        R"(SOFTWARE\Source Dynamics\Source Insight\3.0)",
                                        "keycode",
                                        windowProcData->wParam
                                );
                                logger::log(format(
                                        "Set keycode success: 0x{:08X}, hwnd: 0x{:08X}",
                                        windowProcData->wParam,
                                        reinterpret_cast<uint64_t>(windowProcData->hwnd)
                                ));
                            } catch (runtime_error &e) {
                                logger::log(format(
                                        "Set keycode failed: {}",
                                        e.what()
                                ));
                            }
                        }
                        break;
                    }
                    default: {
                        break;
                    }
                }
                return CallNextHookEx(nullptr, nCode, wParam, lParam);
            }),
            nullptr,
            GetCurrentThreadId()
    );

    return this->_windowHook != nullptr;
}

bool ModuleProxy::unhookWindowProc() {
    return UnhookWindowsHookEx(this->_windowHook);
}
