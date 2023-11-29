#include <components/Configurator.h>
#include <components/WindowManager.h>
#include <types/Key.h>
#include <utils/logger.h>
#include <utils/window.h>

using namespace components;
using namespace std;
using namespace types;
using namespace utils;

WindowManager::WindowManager() : _keyHelper(Configurator::GetInstance()->version().first) {
    _threadDebounceUpdateWindow();
    _threadDebounceRetrieveInfo();
}

WindowManager::~WindowManager() {
    _isRunning.store(false);
}

bool WindowManager::checkNeedCancelWhenLostFocus(const int64_t windowHandle) {
    if (const auto windowClass = window::getWindowClassName(windowHandle);
        windowClass == "si_Poplist") {
        _popListWindowHandle.store(windowHandle);
    }
    else if (_codeWindowHandle >= 0) {
        _debounceUpdateWindowTime.store(chrono::high_resolution_clock::now() + chrono::milliseconds(100));
        _updateWindowHandle.store(-1);
        return true;
    }
    else {
        // Cancel gain focus update if switch back really quick
        _updateWindowHandle.store({});
    }
    return false;
}

bool WindowManager::checkNeedCancelWhenGainFocus(const int64_t windowHandle) {
    if (_codeWindowHandle < 0) {
        _debounceUpdateWindowTime.store(chrono::high_resolution_clock::now() + chrono::milliseconds(1000));
        _updateWindowHandle.store(windowHandle);
    }
    else {
        // Cancel lost focus update if switch back really quick
        _updateWindowHandle.store({});
    }
    if (_popListWindowHandle > 0) {
        _popListWindowHandle.store(-1);
        return true;
    }
    return false;
}

void WindowManager::interactionPaste(const std::any&) {
    _cancelRetrieveInfo();
}

void WindowManager::requestRetrieveInfo() {
    _debounceRetrieveInfoTime.store(chrono::high_resolution_clock::now() + chrono::milliseconds(250));
    _needRetrieveInfo.store(true);
}

bool WindowManager::sendAcceptCompletion() {
    _cancelRetrieveInfo();
    return window::postKeycode(
        _codeWindowHandle,
        _keyHelper.toKeycode(Key::F10, {Modifier::Shift, Modifier::Ctrl, Modifier::Alt})
    );
}

bool WindowManager::sendCancelCompletion() {
    _cancelRetrieveInfo();
    return window::postKeycode(
        _codeWindowHandle,
        _keyHelper.toKeycode(Key::F9, {Modifier::Shift, Modifier::Ctrl, Modifier::Alt})
    );
}

bool WindowManager::sendDoubleInsert() const {
    return window::sendKeycode(_codeWindowHandle, _keyHelper.toKeycode(Key::Insert)) &&
           window::sendKeycode(_codeWindowHandle, _keyHelper.toKeycode(Key::Insert));
}

bool WindowManager::sendInsertCompletion() {
    _cancelRetrieveInfo();
    return window::postKeycode(
        _codeWindowHandle,
        _keyHelper.toKeycode(Key::F12, {Modifier::Shift, Modifier::Ctrl, Modifier::Alt})
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

void WindowManager::_threadDebounceUpdateWindow() {
    thread([this] {
        while (_isRunning.load()) {
            if (const auto updateWindowHandle = _updateWindowHandle.load(); updateWindowHandle) {
                if (const auto deltaTime = _debounceUpdateWindowTime.load() - chrono::high_resolution_clock::now();
                    deltaTime <= chrono::nanoseconds(0)) {
                    logger::log(updateWindowHandle > 0 ? "Gained focus" : "Lost focus");
                    _codeWindowHandle.store(updateWindowHandle);
                    _updateWindowHandle.store({});
                }
                else {
                    this_thread::sleep_for(deltaTime);
                }
            }
            else {
                this_thread::sleep_for(chrono::milliseconds(10));
            }
        }
    }).detach();
}

void WindowManager::_threadDebounceRetrieveInfo() {
    thread([this] {
        while (_isRunning.load()) {
            if (_needRetrieveInfo.load()) {
                if (const auto deltaTime = _debounceRetrieveInfoTime.load() - chrono::high_resolution_clock::now();
                    deltaTime <= chrono::nanoseconds(0)) {
                    logger::log("Sending retrieve info...");
                    window::postKeycode(
                        _codeWindowHandle,
                        _keyHelper.toKeycode(Key::F11, {Modifier::Shift, Modifier::Ctrl, Modifier::Alt})
                    );
                    _needRetrieveInfo.store(false);
                }
                else {
                    this_thread::sleep_for(deltaTime);
                }
            }
            else {
                this_thread::sleep_for(chrono::milliseconds(10));
            }
        }
    }).detach();
}
