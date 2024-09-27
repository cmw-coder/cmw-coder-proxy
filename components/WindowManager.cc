#include <components/ConfigManager.h>
#include <components/MemoryManipulator.h>
#include <components/WindowManager.h>
#include <types/keys.h>
#include <utils/logger.h>
#include <utils/system.h>
#include <utils/window.h>

#include <windows.h>

using namespace components;
using namespace models;
using namespace std;
using namespace types;
using namespace utils;

namespace {
    const auto versionText = format("v{}", VERSION_STRING);
    atomic<uint32_t> mainWindowHandle;
}

WindowManager::WindowManager() {
    _threadInitMenuHandle();

    logger::info("WindowManager is initialized");
}

WindowManager::~WindowManager() {
    _isRunning.store(false);
}

bool WindowManager::checkNeedHideWhenLostFocus(const uint32_t windowHandle) {
    const auto windowClass = window::getWindowClassName(windowHandle);
    logger::debug(format("Target window class: {}", windowClass));
    if (windowClass == "si_Poplist") {
        _popListWindowHandle.store(windowHandle);
    } else {
        unique_lock _currentWindowHandleLock(_codeWindowHandleMutex);
        if (_codeWindowHandle >= 0) {
            _codeWindowHandle = -1;
            return true;
        }
    }
    return false;
}

bool WindowManager::checkNeedShowWhenGainFocus(const uint32_t windowHandle) {
    if (_popListWindowHandle > 0) {
        _popListWindowHandle.store(-1);
    } else {
        if (!hasCodeWindow()) {
            _addEditorWindowHandle(windowHandle);
            unique_lock _currentWindowHandleLock(_codeWindowHandleMutex);
            _codeWindowHandle = windowHandle;
            return true;
        }
    }
    return false;
}

void WindowManager::closeWindowHandle(const uint32_t windowHandle) {
    bool hasAssociatedFileHandle; {
        shared_lock lock(_fileHandleMapMutex);
        hasAssociatedFileHandle = _fileHandleMap.contains(windowHandle);
    }
    if (hasAssociatedFileHandle) {
        unique_lock lock(_fileHandleMapMutex);
        _fileHandleMap.erase(windowHandle);
    }
}

optional<uint32_t> WindowManager::getAssociatedFileHandle(const uint32_t windowHandle) const {
    try {
        shared_lock lock(_fileHandleMapMutex);
        return _fileHandleMap.at(windowHandle);
    } catch (...) {
        return nullopt;
    }
}

tuple<int64_t, int64_t> WindowManager::getClientPosition() const {
    shared_lock _currentWindowHandleLock(_codeWindowHandleMutex);
    if (_codeWindowHandle > 0) {
        return window::getClientPosition(_codeWindowHandle);
    }
    return {};
}

tuple<int64_t, shared_lock<shared_mutex>> WindowManager::sharedAccessCodeWindowHandle() const {
    return {_codeWindowHandle, shared_lock(_codeWindowHandleMutex)};
}

bool WindowManager::hasCodeWindow() const {
    shared_lock lock(_codeWindowHandleMutex);
    return _codeWindowHandle > 0;
}

bool WindowManager::hasPopListWindow() const {
    return _popListWindowHandle.load() > 0;
}

void WindowManager::sendEnd() const {
    shared_lock _currentWindowHandleLock(_codeWindowHandleMutex);
    if (_codeWindowHandle > 0) {
        window::sendKeyInput(VK_END);
    }
}

void WindowManager::sendEscape() const {
    if (hasPopListWindow()) {
        window::sendKeyInput(VK_ESCAPE);
    }
}

void WindowManager::sendF13() const {
    shared_lock _currentWindowHandleLock(_codeWindowHandleMutex);
    if (_codeWindowHandle > 0) {
        window::sendKeyInput(VK_F13);
    }
}

void WindowManager::sendLeftThenRight() const {
    shared_lock _currentWindowHandleLock(_codeWindowHandleMutex);
    if (_codeWindowHandle > 0) {
        window::sendKeyInput(VK_LEFT);
        window::sendKeyInput(VK_RIGHT);
    }
}

bool WindowManager::sendSave() const {
    shared_lock _currentWindowHandleLock(_codeWindowHandleMutex);
    if (_codeWindowHandle > 0) {
        window::sendKeyInput('S', {Modifier::Ctrl});
    }
    return false;
}

bool WindowManager::sendFocus() const {
    shared_lock _currentWindowHandleLock(_codeWindowHandleMutex);
    if (_codeWindowHandle > 0) {
        return SetFocus(reinterpret_cast<HWND>(_codeWindowHandle));
    }
    return false;
}

void WindowManager::setMenuText(const string& text) const {
    if (_menuHandle < 0) {
        return;
    }
    ModifyMenu(
        reinterpret_cast<HMENU>(_menuHandle.load()),
        _menuItemIndex,
        MF_DISABLED,
        _menuItemIndex,
        (_menuBaseText + text).c_str()
    );
    DrawMenuBar(reinterpret_cast<HWND>(mainWindowHandle.load()));
}

void WindowManager::unsetMenuText() const {
    setMenuText(versionText);
}

void WindowManager::_addEditorWindowHandle(const uint32_t windowHandle) {
    unique_lock lock(_fileHandleMapMutex);
    _fileHandleMap.emplace(
        windowHandle,
        MemoryManipulator::GetInstance()->getHandle(MemoryAddress::HandleType::File)
    );
}

void WindowManager::_threadInitMenuHandle() {
    thread([this] {
        mainWindowHandle = window::getMainWindowHandle(GetCurrentProcessId());
        while (!mainWindowHandle) {
            this_thread::sleep_for(chrono::milliseconds(10));
            mainWindowHandle = window::getMainWindowHandle(GetCurrentProcessId());
        }
        const auto menuHandle = GetMenu(reinterpret_cast<HWND>(mainWindowHandle.load()));
        _menuHandle.store(reinterpret_cast<int64_t>(menuHandle));
        _menuItemIndex = GetMenuItemCount(menuHandle);
        AppendMenu(
            menuHandle,
            MF_DISABLED,
            _menuItemIndex,
            (_menuBaseText + versionText).c_str()
        );
    }).detach();
}
