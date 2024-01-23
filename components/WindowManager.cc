#include <components/Configurator.h>
#include <components/WindowManager.h>
#include <types/Key.h>
#include <utils/logger.h>
#include <utils/window.h>

#include <windows.h>

using namespace components;
using namespace std;
using namespace types;
using namespace utils;

WindowManager::WindowManager()
    : _keyHelper(Configurator::GetInstance()->version().first),
      _mainWindowHandle(reinterpret_cast<int64_t>(GetActiveWindow())) {
    const auto menuHandle = GetMenu(reinterpret_cast<HWND>(_mainWindowHandle.load()));
    _menuItemIndex = GetMenuItemCount(menuHandle);
    AppendMenu(menuHandle, MF_DISABLED, _menuItemIndex, format("Comware Coder v{}", VERSION_STRING).c_str());
    _threadDebounceRetrieveInfo();
}

WindowManager::~WindowManager() {
    _isRunning.store(false);
}

bool WindowManager::checkNeedHideWhenLostFocus(const int64_t windowHandle) {
    if (const auto windowClass = window::getWindowClassName(windowHandle);
        windowClass == "si_Poplist") {
        _popListWindowHandle.store(windowHandle);
    } else if (_codeWindowHandle >= 0) {
        logger::debug("Lost focus");
        _codeWindowHandle.store(-1);
        return true;
    }
    return false;
}

bool WindowManager::checkNeedShowWhenGainFocus(const int64_t windowHandle) {
    if (_popListWindowHandle > 0) {
        _popListWindowHandle.store(-1);
    } else if (_codeWindowHandle < 0) {
        logger::debug("Gained focus");
        _codeWindowHandle.store(windowHandle);
        return true;
    }
    return false;
}

tuple<int64_t, int64_t> WindowManager::getClientPosition() const {
    return window::getClientPosition(_codeWindowHandle);
}

bool WindowManager::hasPopListWindow() const {
    return _popListWindowHandle.load() > 0;
}

bool WindowManager::hasValidCodeWindow() const {
    return _codeWindowHandle.load() > 0;
}

void WindowManager::interactionPaste(const any&) {
    _cancelRetrieveInfo();
}

void WindowManager::requestRetrieveInfo() {
    _debounceRetrieveInfoTime.store(chrono::high_resolution_clock::now() + chrono::milliseconds(250));
    _needRetrieveInfo.store(true);
}

void WindowManager::sendF13() const {
    window::sendKeycode(
        _codeWindowHandle,
        _keyHelper.toKeycode(Key::F13)
    );
}

void WindowManager::sendLeftThenRight() const {
    window::sendKeycode(
        _codeWindowHandle,
        _keyHelper.toKeycode(Key::Left)
    );
    window::sendKeycode(
        _codeWindowHandle,
        _keyHelper.toKeycode(Key::Right)
    );
}

bool WindowManager::sendSave() {
    _cancelRetrieveInfo();
    return window::postKeycode(
        _codeWindowHandle,
        _keyHelper.toKeycode(Key::S, Modifier::Ctrl)
    );
}

bool WindowManager::sendUndo() {
    _cancelRetrieveInfo();
    return window::postKeycode(
        _codeWindowHandle,
        _keyHelper.toKeycode(Key::Z, Modifier::Ctrl)
    );
}

void WindowManager::_cancelRetrieveInfo() {
    _needRetrieveInfo.store(false);
}

void WindowManager::_threadDebounceRetrieveInfo() {
    thread([this] {
        while (_isRunning.load()) {
            if (_needRetrieveInfo.load()) {
                if (const auto deltaTime = _debounceRetrieveInfoTime.load() - chrono::high_resolution_clock::now();
                    deltaTime <= chrono::nanoseconds(0)) {
                    logger::info("Retrieve info from Source Insight");
                    window::postKeycode(
                        _codeWindowHandle,
                        _keyHelper.toKeycode(Key::F11, {Modifier::Shift, Modifier::Ctrl, Modifier::Alt})
                    );
                    _needRetrieveInfo.store(false);
                } else {
                    this_thread::sleep_for(deltaTime);
                }
            } else {
                this_thread::sleep_for(chrono::milliseconds(10));
            }
        }
    }).detach();
}
